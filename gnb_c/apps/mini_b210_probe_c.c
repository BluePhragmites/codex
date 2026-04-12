#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "mini_gnb_c/config/config_loader.h"
#include "mini_gnb_c/radio/b210_uhd_probe.h"

typedef enum {
  MINI_GNB_C_B210_MODE_RX = 0,
  MINI_GNB_C_B210_MODE_TX = 1,
  MINI_GNB_C_B210_MODE_TRX = 2
} mini_gnb_c_b210_mode_t;

static void mini_gnb_c_print_b210_probe_help(const char* program) {
  fprintf(stderr,
          "Usage: %s [options]\n"
          "\n"
          "Minimal USRP B210 RX/TX smoke probe for Stage 1 bring-up.\n"
          "\n"
          "Common options:\n"
          "  --mode <rx|tx|trx>     Probe direction, default rx\n"
          "  --args <text>          UHD device args, for example \"serial=8000963\"\n"
          "  --config <yaml>        Load common RF defaults from a gnb_c YAML config first\n"
          "  --subdev <text>        Subdevice string, for example \"A:A\"\n"
          "  --ref <text>           Clock reference source, default \"external\"\n"
          "  --rate <hz>            Sample rate, default 20000000\n"
          "  --freq <hz>            Center frequency, default 2462000000\n"
          "  --rx-freq <hz>         RX frequency override\n"
          "  --tx-freq <hz>         TX frequency override\n"
          "  --gain <db>            Apply one gain value to both RX and TX, default 60\n"
          "  --rx-gain <db>         RX gain override\n"
          "  --tx-gain <db>         TX gain override\n"
          "  --bw <hz>              Bandwidth, default 20000000\n"
          "  --channel <index>      Channel index, default 0\n"
          "  --channel-count <n>    Number of channels, default 1, max 2\n"
          "  --cpu-core <index>     Pin the active worker thread to one CPU core\n"
          "  --rx-cpu-core <index>  Pin the RX worker thread to one CPU core\n"
          "  --tx-cpu-core <index>  Pin the TX worker thread to one CPU core\n"
          "  --skip-host-tuning     Skip the built-in B210 host tuning step\n"
          "  --ring-map <path>      Use a single-file sc16 ring map instead of a raw IQ file\n"
          "  --rx-ring-map <path>   RX ring-map output, mainly for TRX mode\n"
          "  --tx-ring-map <path>   TX ring-map input, mainly for TRX mode\n"
          "  --ring-block-samples <count>\n"
          "                         Samples per ring block, default 4096\n"
          "  --ring-block-count <count>\n"
          "                         Ring block count, default 1024\n"
          "  --duration-mode <samples|wallclock>\n"
          "                         Duration semantics for rx/trx, default samples\n"
          "  --tx-prefetch-samples <count>\n"
          "                         TX ring replay prefetch window in samples for tx/trx ring replay, default auto\n"
          "  --no-ref-lock          Do not fail when ref_locked is false\n"
          "  --no-lo-lock           Do not fail when lo_locked is false\n"
          "\n"
          "RX mode options:\n"
          "  --duration <sec>       Capture duration, default 1\n"
          "  --file <path>          RX output IQ file, default /dev/shm/b210_probe_rx_fc32.dat\n"
          "  --output-file <path>   Same as --file in RX mode\n"
          "\n"
          "TX mode options:\n"
          "  --file <path>          TX input IQ file, default /dev/shm/test.dat\n"
          "  --input-file <path>    Same as --file in TX mode\n"
          "  --tx-file <path>       Explicit TX input path for TRX preparation\n"
          "  --rx-file <path>       Explicit RX output path for TRX preparation\n"
          "\n"
          "  --help                 Print this message\n",
          program);
}

static int mini_gnb_c_parse_double_option(const char* text, double* out) {
  char* end = NULL;

  if (text == NULL || out == NULL) {
    return -1;
  }
  *out = strtod(text, &end);
  if (end == text || *end != '\0') {
    return -1;
  }
  return 0;
}

static int mini_gnb_c_parse_long_option(const char* text, long* out) {
  char* end = NULL;

  if (text == NULL || out == NULL) {
    return -1;
  }
  *out = strtol(text, &end, 10);
  if (end == text || *end != '\0') {
    return -1;
  }
  return 0;
}

static int mini_gnb_c_parse_mode_option(const char* text, mini_gnb_c_b210_mode_t* out) {
  if (text == NULL || out == NULL) {
    return -1;
  }
  if (strcmp(text, "rx") == 0) {
    *out = MINI_GNB_C_B210_MODE_RX;
    return 0;
  }
  if (strcmp(text, "tx") == 0) {
    *out = MINI_GNB_C_B210_MODE_TX;
    return 0;
  }
  if (strcmp(text, "trx") == 0) {
    *out = MINI_GNB_C_B210_MODE_TRX;
    return 0;
  }
  return -1;
}

static int mini_gnb_c_parse_duration_mode_option(const char* text, mini_gnb_c_b210_duration_mode_t* out) {
  if (text == NULL || out == NULL) {
    return -1;
  }
  if (strcmp(text, "samples") == 0) {
    *out = MINI_GNB_C_B210_DURATION_MODE_SAMPLE_TARGET;
    return 0;
  }
  if (strcmp(text, "wallclock") == 0) {
    *out = MINI_GNB_C_B210_DURATION_MODE_WALLCLOCK;
    return 0;
  }
  return -1;
}

static int mini_gnb_c_ensure_directory_recursive(const char* path) {
  char temp[MINI_GNB_C_MAX_PATH];
  size_t len = 0u;
  size_t i = 0u;

  if (path == NULL || path[0] == '\0') {
    return -1;
  }
  if (snprintf(temp, sizeof(temp), "%s", path) >= (int)sizeof(temp)) {
    return -1;
  }
  len = strlen(temp);
  if (len == 0u) {
    return -1;
  }

  for (i = 1u; i < len; ++i) {
    if (temp[i] == '/') {
      temp[i] = '\0';
      if (strlen(temp) > 0u && mkdir(temp, 0777) != 0 && errno != EEXIST) {
        return -1;
      }
      temp[i] = '/';
    }
  }
  if (mkdir(temp, 0777) != 0 && errno != EEXIST) {
    return -1;
  }
  return 0;
}

static int mini_gnb_c_ensure_parent_directory(const char* path) {
  char parent[MINI_GNB_C_MAX_PATH];
  char* slash = NULL;

  if (path == NULL || path[0] == '\0') {
    return -1;
  }
  if (snprintf(parent, sizeof(parent), "%s", path) >= (int)sizeof(parent)) {
    return -1;
  }
  slash = strrchr(parent, '/');
  if (slash == NULL) {
    return 0;
  }
  if (slash == parent) {
    return 0;
  }
  *slash = '\0';
  return mini_gnb_c_ensure_directory_recursive(parent);
}

static const char* mini_gnb_c_find_config_option(int argc, char** argv) {
  int i = 0;

  if (argv == NULL) {
    return NULL;
  }
  for (i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--config") == 0) {
      if ((i + 1) < argc) {
        return argv[i + 1];
      }
      return NULL;
    }
    if (strncmp(argv[i], "--config=", 9) == 0) {
      return argv[i] + 9;
    }
  }
  return NULL;
}

static void mini_gnb_c_apply_yaml_to_b210_probe_configs(const mini_gnb_c_config_t* config,
                                                        mini_gnb_c_b210_probe_config_t* rx_config,
                                                        mini_gnb_c_b210_tx_config_t* tx_config,
                                                        mini_gnb_c_b210_trx_config_t* trx_config) {
  if (config == NULL) {
    return;
  }
  if (rx_config != NULL) {
    (void)snprintf(rx_config->device_args, sizeof(rx_config->device_args), "%s", config->rf.device_args);
    (void)snprintf(rx_config->subdev, sizeof(rx_config->subdev), "%s", config->rf.subdev);
    (void)snprintf(rx_config->ref, sizeof(rx_config->ref), "%s", config->rf.clock_src);
    rx_config->rate_sps = config->rf.srate;
    rx_config->freq_hz = config->rf.rx_freq_hz;
    rx_config->gain_db = config->rf.rx_gain;
    rx_config->cpu_core = config->rf.rx_cpu_core;
  }
  if (tx_config != NULL) {
    (void)snprintf(tx_config->device_args, sizeof(tx_config->device_args), "%s", config->rf.device_args);
    (void)snprintf(tx_config->subdev, sizeof(tx_config->subdev), "%s", config->rf.subdev);
    (void)snprintf(tx_config->ref, sizeof(tx_config->ref), "%s", config->rf.clock_src);
    tx_config->rate_sps = config->rf.srate;
    tx_config->freq_hz = config->rf.tx_freq_hz;
    tx_config->gain_db = config->rf.tx_gain;
    tx_config->cpu_core = config->rf.tx_cpu_core;
    tx_config->tx_prefetch_samples = config->rf.tx_prefetch_samples;
  }
  if (trx_config != NULL) {
    (void)snprintf(trx_config->device_args, sizeof(trx_config->device_args), "%s", config->rf.device_args);
    (void)snprintf(trx_config->subdev, sizeof(trx_config->subdev), "%s", config->rf.subdev);
    (void)snprintf(trx_config->ref, sizeof(trx_config->ref), "%s", config->rf.clock_src);
    trx_config->rate_sps = config->rf.srate;
    trx_config->rx_freq_hz = config->rf.rx_freq_hz;
    trx_config->tx_freq_hz = config->rf.tx_freq_hz;
    trx_config->rx_gain_db = config->rf.rx_gain;
    trx_config->tx_gain_db = config->rf.tx_gain;
    trx_config->rx_cpu_core = config->rf.rx_cpu_core;
    trx_config->tx_cpu_core = config->rf.tx_cpu_core;
    trx_config->tx_prefetch_samples = config->rf.tx_prefetch_samples;
  }
}

int main(int argc, char** argv) {
  mini_gnb_c_b210_probe_config_t rx_config;
  mini_gnb_c_b210_probe_report_t rx_report;
  mini_gnb_c_b210_tx_config_t tx_config;
  mini_gnb_c_b210_tx_report_t tx_report;
  mini_gnb_c_b210_trx_config_t trx_config;
  mini_gnb_c_b210_trx_report_t trx_report;
  mini_gnb_c_config_t yaml_config;
  mini_gnb_c_b210_mode_t mode = MINI_GNB_C_B210_MODE_RX;
  char error_message[256];
  const char* config_path = NULL;
  int option_index = 0;
  int option = 0;

  static const struct option long_options[] = {
      {"mode", required_argument, NULL, 'm'},
      {"args", required_argument, NULL, 'a'},
      {"config", required_argument, NULL, 'C'},
      {"subdev", required_argument, NULL, 's'},
      {"ref", required_argument, NULL, 'r'},
      {"rate", required_argument, NULL, 'R'},
      {"freq", required_argument, NULL, 'f'},
      {"rx-freq", required_argument, NULL, 1016},
      {"tx-freq", required_argument, NULL, 1017},
      {"gain", required_argument, NULL, 'g'},
      {"rx-gain", required_argument, NULL, 1013},
      {"tx-gain", required_argument, NULL, 1014},
      {"bw", required_argument, NULL, 'b'},
      {"duration", required_argument, NULL, 'd'},
      {"channel", required_argument, NULL, 'c'},
      {"channels", required_argument, NULL, 'c'},
      {"channel-count", required_argument, NULL, 1006},
      {"cpu-core", required_argument, NULL, 'p'},
      {"rx-cpu-core", required_argument, NULL, 1007},
      {"tx-cpu-core", required_argument, NULL, 1008},
      {"skip-host-tuning", no_argument, NULL, 1002},
      {"ring-map", required_argument, NULL, 1003},
      {"rx-ring-map", required_argument, NULL, 1009},
      {"tx-ring-map", required_argument, NULL, 1010},
      {"ring-block-samples", required_argument, NULL, 1004},
      {"ring-block-count", required_argument, NULL, 1005},
      {"duration-mode", required_argument, NULL, 1018},
      {"tx-prefetch-samples", required_argument, NULL, 1015},
      {"file", required_argument, NULL, 'o'},
      {"output-file", required_argument, NULL, 'o'},
      {"input-file", required_argument, NULL, 'i'},
      {"rx-file", required_argument, NULL, 1011},
      {"tx-file", required_argument, NULL, 1012},
      {"no-ref-lock", no_argument, NULL, 1000},
      {"no-lo-lock", no_argument, NULL, 1001},
      {"help", no_argument, NULL, 'h'},
      {0, 0, 0, 0},
  };

  mini_gnb_c_b210_probe_config_init(&rx_config);
  mini_gnb_c_b210_tx_config_init(&tx_config);
  mini_gnb_c_b210_trx_config_init(&trx_config);
  memset(&yaml_config, 0, sizeof(yaml_config));

  config_path = mini_gnb_c_find_config_option(argc, argv);
  if (config_path != NULL && config_path[0] != '\0') {
    if (mini_gnb_c_load_config(config_path, &yaml_config, error_message, sizeof(error_message)) != 0) {
      fprintf(stderr, "failed to load --config %s: %s\n", config_path, error_message);
      return 1;
    }
    mini_gnb_c_apply_yaml_to_b210_probe_configs(&yaml_config, &rx_config, &tx_config, &trx_config);
  }

  while ((option = getopt_long(argc, argv, "m:a:C:s:r:R:f:g:b:d:c:p:o:i:h", long_options, &option_index)) != -1) {
    long parsed_long = 0;
    double parsed_double = 0.0;

    switch (option) {
      case 'm':
        if (mini_gnb_c_parse_mode_option(optarg, &mode) != 0) {
          fprintf(stderr, "invalid --mode value: %s\n", optarg);
          return 1;
        }
        break;
      case 'a':
        (void)snprintf(rx_config.device_args, sizeof(rx_config.device_args), "%s", optarg);
        (void)snprintf(tx_config.device_args, sizeof(tx_config.device_args), "%s", optarg);
        (void)snprintf(trx_config.device_args, sizeof(trx_config.device_args), "%s", optarg);
        break;
      case 'C':
        break;
      case 's':
        (void)snprintf(rx_config.subdev, sizeof(rx_config.subdev), "%s", optarg);
        (void)snprintf(tx_config.subdev, sizeof(tx_config.subdev), "%s", optarg);
        (void)snprintf(trx_config.subdev, sizeof(trx_config.subdev), "%s", optarg);
        break;
      case 'r':
        (void)snprintf(rx_config.ref, sizeof(rx_config.ref), "%s", optarg);
        (void)snprintf(tx_config.ref, sizeof(tx_config.ref), "%s", optarg);
        (void)snprintf(trx_config.ref, sizeof(trx_config.ref), "%s", optarg);
        break;
      case 'R':
        if (mini_gnb_c_parse_double_option(optarg, &rx_config.rate_sps) != 0 ||
            mini_gnb_c_parse_double_option(optarg, &tx_config.rate_sps) != 0 ||
            mini_gnb_c_parse_double_option(optarg, &trx_config.rate_sps) != 0) {
          fprintf(stderr, "invalid --rate value: %s\n", optarg);
          return 1;
        }
        break;
      case 'f':
        if (mini_gnb_c_parse_double_option(optarg, &parsed_double) != 0) {
          fprintf(stderr, "invalid --freq value: %s\n", optarg);
          return 1;
        }
        mini_gnb_c_b210_apply_shared_freq(&rx_config, &tx_config, &trx_config, parsed_double);
        break;
      case 1016:
        if (mini_gnb_c_parse_double_option(optarg, &parsed_double) != 0) {
          fprintf(stderr, "invalid --rx-freq value: %s\n", optarg);
          return 1;
        }
        mini_gnb_c_b210_apply_rx_freq(&rx_config, &trx_config, parsed_double);
        break;
      case 1017:
        if (mini_gnb_c_parse_double_option(optarg, &parsed_double) != 0) {
          fprintf(stderr, "invalid --tx-freq value: %s\n", optarg);
          return 1;
        }
        mini_gnb_c_b210_apply_tx_freq(&tx_config, &trx_config, parsed_double);
        break;
      case 'g':
        if (mini_gnb_c_parse_double_option(optarg, &parsed_double) != 0) {
          fprintf(stderr, "invalid --gain value: %s\n", optarg);
          return 1;
        }
        mini_gnb_c_b210_apply_shared_gain(&rx_config, &tx_config, &trx_config, parsed_double);
        break;
      case 1013:
        if (mini_gnb_c_parse_double_option(optarg, &parsed_double) != 0) {
          fprintf(stderr, "invalid --rx-gain value: %s\n", optarg);
          return 1;
        }
        mini_gnb_c_b210_apply_rx_gain(&rx_config, &trx_config, parsed_double);
        break;
      case 1014:
        if (mini_gnb_c_parse_double_option(optarg, &parsed_double) != 0) {
          fprintf(stderr, "invalid --tx-gain value: %s\n", optarg);
          return 1;
        }
        mini_gnb_c_b210_apply_tx_gain(&tx_config, &trx_config, parsed_double);
        break;
      case 'b':
        if (mini_gnb_c_parse_double_option(optarg, &rx_config.bandwidth_hz) != 0 ||
            mini_gnb_c_parse_double_option(optarg, &tx_config.bandwidth_hz) != 0 ||
            mini_gnb_c_parse_double_option(optarg, &trx_config.bandwidth_hz) != 0) {
          fprintf(stderr, "invalid --bw value: %s\n", optarg);
          return 1;
        }
        break;
      case 'd':
        if (mini_gnb_c_parse_double_option(optarg, &rx_config.duration_sec) != 0 ||
            mini_gnb_c_parse_double_option(optarg, &trx_config.duration_sec) != 0) {
          fprintf(stderr, "invalid --duration value: %s\n", optarg);
          return 1;
        }
        break;
      case 'c':
        if (mini_gnb_c_parse_long_option(optarg, &parsed_long) != 0 || parsed_long < 0) {
          fprintf(stderr, "invalid --channel value: %s\n", optarg);
          return 1;
        }
        rx_config.channel = (size_t)parsed_long;
        tx_config.channel = (size_t)parsed_long;
        trx_config.channel = (size_t)parsed_long;
        break;
      case 1006:
        if (mini_gnb_c_parse_long_option(optarg, &parsed_long) != 0 || parsed_long <= 0) {
          fprintf(stderr, "invalid --channel-count value: %s\n", optarg);
          return 1;
        }
        rx_config.channel_count = (uint32_t)parsed_long;
        tx_config.channel_count = (uint32_t)parsed_long;
        trx_config.channel_count = (uint32_t)parsed_long;
        break;
      case 'p':
        if (mini_gnb_c_parse_long_option(optarg, &parsed_long) != 0) {
          fprintf(stderr, "invalid --cpu-core value: %s\n", optarg);
          return 1;
        }
        rx_config.cpu_core = (int)parsed_long;
        tx_config.cpu_core = (int)parsed_long;
        trx_config.rx_cpu_core = (int)parsed_long;
        trx_config.tx_cpu_core = (int)parsed_long;
        break;
      case 1007:
        if (mini_gnb_c_parse_long_option(optarg, &parsed_long) != 0) {
          fprintf(stderr, "invalid --rx-cpu-core value: %s\n", optarg);
          return 1;
        }
        trx_config.rx_cpu_core = (int)parsed_long;
        break;
      case 1008:
        if (mini_gnb_c_parse_long_option(optarg, &parsed_long) != 0) {
          fprintf(stderr, "invalid --tx-cpu-core value: %s\n", optarg);
          return 1;
        }
        trx_config.tx_cpu_core = (int)parsed_long;
        break;
      case 1002:
        rx_config.apply_host_tuning = false;
        tx_config.apply_host_tuning = false;
        trx_config.apply_host_tuning = false;
        break;
      case 1003:
        (void)snprintf(rx_config.ring_path, sizeof(rx_config.ring_path), "%s", optarg);
        (void)snprintf(tx_config.ring_path, sizeof(tx_config.ring_path), "%s", optarg);
        break;
      case 1009:
        (void)snprintf(trx_config.rx_ring_path, sizeof(trx_config.rx_ring_path), "%s", optarg);
        break;
      case 1010:
        (void)snprintf(trx_config.tx_ring_path, sizeof(trx_config.tx_ring_path), "%s", optarg);
        break;
      case 1004:
        if (mini_gnb_c_parse_long_option(optarg, &parsed_long) != 0 || parsed_long <= 0) {
          fprintf(stderr, "invalid --ring-block-samples value: %s\n", optarg);
          return 1;
        }
        rx_config.ring_block_samples = (uint32_t)parsed_long;
        tx_config.ring_block_samples = (uint32_t)parsed_long;
        trx_config.ring_block_samples = (uint32_t)parsed_long;
        break;
      case 1005:
        if (mini_gnb_c_parse_long_option(optarg, &parsed_long) != 0 || parsed_long <= 0) {
          fprintf(stderr, "invalid --ring-block-count value: %s\n", optarg);
          return 1;
        }
        rx_config.ring_block_count = (uint32_t)parsed_long;
        tx_config.ring_block_count = (uint32_t)parsed_long;
        trx_config.ring_block_count = (uint32_t)parsed_long;
        break;
      case 1018:
        if (mini_gnb_c_parse_duration_mode_option(optarg, &rx_config.duration_mode) != 0 ||
            mini_gnb_c_parse_duration_mode_option(optarg, &trx_config.duration_mode) != 0) {
          fprintf(stderr, "invalid --duration-mode value: %s\n", optarg);
          return 1;
        }
        break;
      case 1015:
        if (mini_gnb_c_parse_long_option(optarg, &parsed_long) != 0 || parsed_long <= 0) {
          fprintf(stderr, "invalid --tx-prefetch-samples value: %s\n", optarg);
          return 1;
        }
        tx_config.tx_prefetch_samples = (uint32_t)parsed_long;
        trx_config.tx_prefetch_samples = (uint32_t)parsed_long;
        break;
      case 'o':
        if (mode == MINI_GNB_C_B210_MODE_TX) {
          (void)snprintf(tx_config.input_path, sizeof(tx_config.input_path), "%s", optarg);
        } else if (mode == MINI_GNB_C_B210_MODE_TRX) {
          (void)snprintf(trx_config.tx_input_path, sizeof(trx_config.tx_input_path), "%s", optarg);
        } else {
          (void)snprintf(rx_config.output_path, sizeof(rx_config.output_path), "%s", optarg);
        }
        break;
      case 'i':
        (void)snprintf(tx_config.input_path, sizeof(tx_config.input_path), "%s", optarg);
        break;
      case 1011:
        (void)snprintf(trx_config.rx_output_path, sizeof(trx_config.rx_output_path), "%s", optarg);
        break;
      case 1012:
        (void)snprintf(trx_config.tx_input_path, sizeof(trx_config.tx_input_path), "%s", optarg);
        break;
      case 1000:
        rx_config.require_ref_lock = false;
        tx_config.require_ref_lock = false;
        trx_config.require_ref_lock = false;
        break;
      case 1001:
        rx_config.require_lo_lock = false;
        tx_config.require_lo_lock = false;
        trx_config.require_lo_lock = false;
        break;
      case 'h':
        mini_gnb_c_print_b210_probe_help(argv[0]);
        return 0;
      default:
        mini_gnb_c_print_b210_probe_help(argv[0]);
        return 1;
    }
  }

  if (mode == MINI_GNB_C_B210_MODE_TRX) {
    if (trx_config.rx_ring_path[0] == '\0' || trx_config.tx_ring_path[0] == '\0') {
      fprintf(stderr, "TRX mode currently requires both --rx-ring-map and --tx-ring-map\n");
      return 1;
    }
    if (mini_gnb_c_ensure_parent_directory(trx_config.rx_ring_path) != 0) {
      fprintf(stderr, "failed to prepare output directory for %s\n", trx_config.rx_ring_path);
      return 1;
    }
    if (mini_gnb_c_b210_trx_run(&trx_config, &trx_report, error_message, sizeof(error_message)) != 0) {
      fprintf(stderr, "mini_b210_probe_c TRX failed: %s\n", error_message);
      return 1;
    }

    printf("mini_b210_probe_c completed successfully\n");
    printf("  mode=trx\n");
    printf("  config=%s\n", (config_path != NULL && config_path[0] != '\0') ? config_path : "(none)");
    printf("  device_args=%s\n", trx_config.device_args[0] != '\0' ? trx_config.device_args : "(default)");
    printf("  subdev=%s\n", trx_config.subdev[0] != '\0' ? trx_config.subdev : "(default)");
    printf("  ref=%s\n", trx_config.ref[0] != '\0' ? trx_config.ref : "(default)");
    printf("  channel=%zu\n", trx_config.channel);
    printf("  channel_count=%u\n", trx_config.channel_count);
    printf("  rx_ring=%s\n", trx_config.rx_ring_path);
    printf("  tx_ring=%s\n", trx_config.tx_ring_path);
    printf("  rx_cpu_core=%d\n", trx_config.rx_cpu_core);
    printf("  tx_cpu_core=%d\n", trx_config.tx_cpu_core);
    printf("  host_tuning=%s\n", trx_report.host_tuning_summary);
    printf("  requested_samples=%zu\n", trx_report.requested_samples);
    printf("  received_samples=%zu\n", trx_report.received_samples);
    printf("  transmitted_samples=%zu\n", trx_report.transmitted_samples);
    printf("  duration_mode=%s\n",
           trx_report.duration_mode == MINI_GNB_C_B210_DURATION_MODE_WALLCLOCK ? "wallclock" : "samples");
    printf("  wall_elapsed_sec=%.6f\n", trx_report.wall_elapsed_sec);
    printf("  rx_recoverable_events=%zu\n", trx_report.rx_recoverable_events);
    printf("  rx_overflow_events=%zu\n", trx_report.rx_overflow_events);
    printf("  rx_timeout_events=%zu\n", trx_report.rx_timeout_events);
    printf("  rx_gap_events=%zu\n", trx_report.rx_gap_events);
    printf("  rx_lost_samples_estimate=%zu\n", trx_report.rx_lost_samples_estimate);
    printf("  rx_ring_blocks=%zu\n", trx_report.rx_ring_blocks_committed);
    printf("  tx_ring_blocks=%zu\n", trx_report.tx_ring_blocks_committed);
    printf("  tx_prefetch_samples=%zu\n", trx_report.tx_prefetch_samples);
    printf("  actual_rate_sps=%.0f\n", trx_report.actual_rate_sps);
    printf("  actual_rx_freq_hz=%.0f\n", trx_report.actual_rx_freq_hz);
    printf("  actual_tx_freq_hz=%.0f\n", trx_report.actual_tx_freq_hz);
    printf("  actual_rx_gain_db=%.1f\n", trx_report.actual_rx_gain_db);
    printf("  actual_tx_gain_db=%.1f\n", trx_report.actual_tx_gain_db);
    printf("  actual_bandwidth_hz=%.0f\n", trx_report.actual_bandwidth_hz);
    printf("  tx_ring_wrap_count=%zu\n", trx_report.tx_ring_wrap_count);
    printf("  ref_locked=%s\n",
           trx_report.ref_locked_valid ? (trx_report.ref_locked ? "true" : "false") : "(sensor unavailable)");
    printf("  rx_lo_locked=%s\n",
           trx_report.rx_lo_locked_valid ? (trx_report.rx_lo_locked ? "true" : "false") : "(sensor unavailable)");
    printf("  tx_lo_locked=%s\n",
           trx_report.tx_lo_locked_valid ? (trx_report.tx_lo_locked ? "true" : "false") : "(sensor unavailable)");
    printf("  burst_ack=%s\n",
           trx_report.burst_ack_valid ? (trx_report.burst_ack ? "true" : "false") : "(not reported)");
    printf("  underflow_observed=%s\n", trx_report.underflow_observed ? "true" : "false");
    printf("  seq_error_observed=%s\n", trx_report.seq_error_observed ? "true" : "false");
    printf("  time_error_observed=%s\n", trx_report.time_error_observed ? "true" : "false");
    printf("  device_summary=%s\n",
           trx_report.device_summary[0] != '\0' ? trx_report.device_summary : "(unavailable)");
    return 0;
  }

  if (mode == MINI_GNB_C_B210_MODE_TX) {
    if (tx_config.ring_path[0] == '\0' && tx_config.input_path[0] == '\0') {
      fprintf(stderr, "missing TX input file\n");
      return 1;
    }
    if (mini_gnb_c_b210_tx_from_file_run(&tx_config, &tx_report, error_message, sizeof(error_message)) != 0) {
      fprintf(stderr, "mini_b210_probe_c TX failed: %s\n", error_message);
      return 1;
    }

    printf("mini_b210_probe_c completed successfully\n");
    printf("  mode=tx\n");
    printf("  config=%s\n", (config_path != NULL && config_path[0] != '\0') ? config_path : "(none)");
    printf("  device_args=%s\n", tx_config.device_args[0] != '\0' ? tx_config.device_args : "(default)");
    printf("  subdev=%s\n", tx_config.subdev[0] != '\0' ? tx_config.subdev : "(default)");
    printf("  ref=%s\n", tx_config.ref[0] != '\0' ? tx_config.ref : "(default)");
    printf("  channel=%zu\n", tx_config.channel);
    printf("  channel_count=%u\n", tx_config.channel_count);
    printf("  input=%s\n", tx_config.ring_path[0] != '\0' ? tx_config.ring_path : tx_config.input_path);
    printf("  input_kind=%s\n", tx_report.used_ring_map ? "ring-map(sc16)" : "raw-fc32");
    printf("  cpu_core=%d\n", tx_config.cpu_core);
    printf("  host_tuning=%s\n", tx_report.host_tuning_summary);
    printf("  requested_samples=%zu\n", tx_report.requested_samples);
    printf("  transmitted_samples=%zu\n", tx_report.transmitted_samples);
    printf("  ring_blocks=%zu\n", tx_report.ring_blocks_committed);
    printf("  tx_prefetch_samples=%zu\n", tx_report.tx_prefetch_samples);
    printf("  actual_rate_sps=%.0f\n", tx_report.actual_rate_sps);
    printf("  actual_freq_hz=%.0f\n", tx_report.actual_freq_hz);
    printf("  actual_gain_db=%.1f\n", tx_report.actual_gain_db);
    printf("  actual_bandwidth_hz=%.0f\n", tx_report.actual_bandwidth_hz);
    printf("  tx_subdev=%s\n", tx_report.tx_subdev_name[0] != '\0' ? tx_report.tx_subdev_name : "(unknown)");
    printf("  ref_locked=%s\n",
           tx_report.ref_locked_valid ? (tx_report.ref_locked ? "true" : "false") : "(sensor unavailable)");
    printf("  lo_locked=%s\n",
           tx_report.lo_locked_valid ? (tx_report.lo_locked ? "true" : "false") : "(sensor unavailable)");
    printf("  burst_ack=%s\n",
           tx_report.burst_ack_valid ? (tx_report.burst_ack ? "true" : "false") : "(not reported)");
    printf("  underflow_observed=%s\n", tx_report.underflow_observed ? "true" : "false");
    printf("  seq_error_observed=%s\n", tx_report.seq_error_observed ? "true" : "false");
    printf("  time_error_observed=%s\n", tx_report.time_error_observed ? "true" : "false");
    printf("  device_summary=%s\n", tx_report.device_summary[0] != '\0' ? tx_report.device_summary : "(unavailable)");
    return 0;
  }

  if (mini_gnb_c_ensure_parent_directory(rx_config.ring_path[0] != '\0' ? rx_config.ring_path : rx_config.output_path) != 0) {
    fprintf(stderr,
            "failed to prepare output directory for %s\n",
            rx_config.ring_path[0] != '\0' ? rx_config.ring_path : rx_config.output_path);
    return 1;
  }
  if (mini_gnb_c_b210_probe_run(&rx_config, &rx_report, error_message, sizeof(error_message)) != 0) {
    fprintf(stderr, "mini_b210_probe_c RX failed: %s\n", error_message);
    return 1;
  }

  printf("mini_b210_probe_c completed successfully\n");
  printf("  mode=rx\n");
  printf("  config=%s\n", (config_path != NULL && config_path[0] != '\0') ? config_path : "(none)");
  printf("  device_args=%s\n", rx_config.device_args[0] != '\0' ? rx_config.device_args : "(default)");
  printf("  subdev=%s\n", rx_config.subdev[0] != '\0' ? rx_config.subdev : "(default)");
  printf("  ref=%s\n", rx_config.ref[0] != '\0' ? rx_config.ref : "(default)");
  printf("  channel=%zu\n", rx_config.channel);
  printf("  channel_count=%u\n", rx_config.channel_count);
  printf("  output=%s\n", rx_config.ring_path[0] != '\0' ? rx_config.ring_path : rx_config.output_path);
  printf("  output_kind=%s\n", rx_report.used_ring_map ? "ring-map(sc16)" : "raw-fc32");
  printf("  cpu_core=%d\n", rx_config.cpu_core);
  printf("  host_tuning=%s\n", rx_report.host_tuning_summary);
  printf("  requested_samples=%zu\n", rx_report.requested_samples);
  printf("  received_samples=%zu\n", rx_report.received_samples);
  printf("  duration_mode=%s\n",
         rx_report.duration_mode == MINI_GNB_C_B210_DURATION_MODE_WALLCLOCK ? "wallclock" : "samples");
  printf("  wall_elapsed_sec=%.6f\n", rx_report.wall_elapsed_sec);
  printf("  rx_recoverable_events=%zu\n", rx_report.rx_recoverable_events);
  printf("  rx_overflow_events=%zu\n", rx_report.rx_overflow_events);
  printf("  rx_timeout_events=%zu\n", rx_report.rx_timeout_events);
  printf("  rx_gap_events=%zu\n", rx_report.rx_gap_events);
  printf("  rx_lost_samples_estimate=%zu\n", rx_report.rx_lost_samples_estimate);
  printf("  ring_blocks=%zu\n", rx_report.ring_blocks_committed);
  printf("  actual_rate_sps=%.0f\n", rx_report.actual_rate_sps);
  printf("  actual_freq_hz=%.0f\n", rx_report.actual_freq_hz);
  printf("  actual_gain_db=%.1f\n", rx_report.actual_gain_db);
  printf("  actual_bandwidth_hz=%.0f\n", rx_report.actual_bandwidth_hz);
  printf("  rx_subdev=%s\n", rx_report.rx_subdev_name[0] != '\0' ? rx_report.rx_subdev_name : "(unknown)");
  printf("  ref_locked=%s\n",
         rx_report.ref_locked_valid ? (rx_report.ref_locked ? "true" : "false") : "(sensor unavailable)");
  printf("  lo_locked=%s\n",
         rx_report.lo_locked_valid ? (rx_report.lo_locked ? "true" : "false") : "(sensor unavailable)");
  printf("  device_summary=%s\n", rx_report.device_summary[0] != '\0' ? rx_report.device_summary : "(unavailable)");
  return 0;
}
