#include "mini_gnb_c/radio/b210_app_runtime.h"

#include <stdio.h>
#include <string.h>

#include "mini_gnb_c/common/json_utils.h"
#include "mini_gnb_c/common/types.h"

#ifdef MINI_GNB_C_HAVE_UHD
#include "mini_gnb_c/radio/b210_uhd_probe.h"
#endif

static int mini_gnb_c_fail_b210_runtime(char* error_message,
                                        const size_t error_message_size,
                                        const char* message) {
  if (error_message != NULL && error_message_size > 0u) {
    (void)snprintf(error_message, error_message_size, "%s", message != NULL ? message : "unknown error");
  }
  return -1;
}

static bool mini_gnb_c_path_is_absolute(const char* path) {
  if (path == NULL || path[0] == '\0') {
    return false;
  }
  if (path[0] == '/' || path[0] == '\\') {
    return true;
  }
  return path[1] == ':';
}

static int mini_gnb_c_resolve_runtime_path(const char* configured_path,
                                           char* out,
                                           const size_t out_size) {
  if (out == NULL || out_size == 0u) {
    return -1;
  }
  out[0] = '\0';
  if (configured_path == NULL || configured_path[0] == '\0') {
    return 0;
  }
  if (mini_gnb_c_path_is_absolute(configured_path)) {
    return snprintf(out, out_size, "%s", configured_path) < (int)out_size ? 0 : -1;
  }
  return mini_gnb_c_join_path(MINI_GNB_C_SOURCE_DIR, configured_path, out, out_size);
}

static int mini_gnb_c_build_default_shm_path(const char* app_name,
                                             const char* suffix,
                                             char* out,
                                             const size_t out_size) {
  if (app_name == NULL || suffix == NULL || out == NULL || out_size == 0u) {
    return -1;
  }
  return snprintf(out, out_size, "%s/%s_%s", "/dev/shm", app_name, suffix) < (int)out_size ? 0 : -1;
}

static bool mini_gnb_c_driver_is_b210(const char* driver_name) {
  return driver_name != NULL &&
         (strcmp(driver_name, "b210") == 0 || strcmp(driver_name, "uhd") == 0 ||
          strcmp(driver_name, "uhd-b210") == 0);
}

bool mini_gnb_c_rf_app_runtime_requested(const mini_gnb_c_config_t* config) {
  return config != NULL && config->rf.runtime_mode != MINI_GNB_C_RF_RUNTIME_MODE_SIMULATOR;
}

#ifdef MINI_GNB_C_HAVE_UHD

static mini_gnb_c_b210_duration_mode_t mini_gnb_c_b210_duration_mode_from_config(
    const mini_gnb_c_rf_duration_mode_t mode) {
  return mode == MINI_GNB_C_RF_DURATION_MODE_WALLCLOCK ? MINI_GNB_C_B210_DURATION_MODE_WALLCLOCK
                                                       : MINI_GNB_C_B210_DURATION_MODE_SAMPLE_TARGET;
}

static void mini_gnb_c_fill_common_rx_config(mini_gnb_c_b210_probe_config_t* out,
                                             const char* app_name,
                                             const mini_gnb_c_rf_config_t* rf) {
  char resolved[MINI_GNB_C_MAX_PATH];

  mini_gnb_c_b210_probe_config_init(out);
  (void)snprintf(out->device_args, sizeof(out->device_args), "%s", rf->device_args);
  (void)snprintf(out->subdev, sizeof(out->subdev), "%s", rf->subdev);
  (void)snprintf(out->ref, sizeof(out->ref), "%s", rf->clock_src);
  out->rate_sps = rf->srate;
  out->freq_hz = rf->rx_freq_hz;
  out->gain_db = rf->rx_gain;
  out->bandwidth_hz = rf->bandwidth_hz;
  out->duration_sec = rf->duration_sec;
  out->duration_mode = mini_gnb_c_b210_duration_mode_from_config(rf->duration_mode);
  out->channel = rf->channel;
  out->channel_count = rf->channel_count;
  out->cpu_core = rf->rx_cpu_core;
  out->apply_host_tuning = rf->apply_host_tuning;
  out->require_ref_lock = rf->require_ref_lock;
  out->require_lo_lock = rf->require_lo_lock;
  out->ring_block_samples = rf->ring_block_samples;
  out->ring_block_count = rf->ring_block_count;

  if (mini_gnb_c_resolve_runtime_path(rf->rx_output_file, resolved, sizeof(resolved)) == 0 && resolved[0] != '\0') {
    (void)snprintf(out->output_path, sizeof(out->output_path), "%s", resolved);
  } else if (mini_gnb_c_build_default_shm_path(app_name, "rx_fc32.dat", out->output_path, sizeof(out->output_path)) !=
             0) {
    out->output_path[0] = '\0';
  }
  if (mini_gnb_c_resolve_runtime_path(rf->rx_ring_map, resolved, sizeof(resolved)) == 0 && resolved[0] != '\0') {
    (void)snprintf(out->ring_path, sizeof(out->ring_path), "%s", resolved);
  } else if (out->output_path[0] == '\0' &&
             mini_gnb_c_build_default_shm_path(app_name, "rx_ring.map", out->ring_path, sizeof(out->ring_path)) != 0) {
    out->ring_path[0] = '\0';
  }
}

static void mini_gnb_c_fill_common_tx_config(mini_gnb_c_b210_tx_config_t* out,
                                             const mini_gnb_c_rf_config_t* rf) {
  char resolved[MINI_GNB_C_MAX_PATH];

  mini_gnb_c_b210_tx_config_init(out);
  (void)snprintf(out->device_args, sizeof(out->device_args), "%s", rf->device_args);
  (void)snprintf(out->subdev, sizeof(out->subdev), "%s", rf->subdev);
  (void)snprintf(out->ref, sizeof(out->ref), "%s", rf->clock_src);
  out->rate_sps = rf->srate;
  out->freq_hz = rf->tx_freq_hz;
  out->gain_db = rf->tx_gain;
  out->bandwidth_hz = rf->bandwidth_hz;
  out->channel = rf->channel;
  out->channel_count = rf->channel_count;
  out->cpu_core = rf->tx_cpu_core;
  out->apply_host_tuning = rf->apply_host_tuning;
  out->require_ref_lock = rf->require_ref_lock;
  out->require_lo_lock = rf->require_lo_lock;
  out->ring_block_samples = rf->ring_block_samples;
  out->ring_block_count = rf->ring_block_count;
  out->tx_prefetch_samples = rf->tx_prefetch_samples;

  if (mini_gnb_c_resolve_runtime_path(rf->tx_input_file, resolved, sizeof(resolved)) == 0 && resolved[0] != '\0') {
    (void)snprintf(out->input_path, sizeof(out->input_path), "%s", resolved);
  } else {
    out->input_path[0] = '\0';
  }
  if (mini_gnb_c_resolve_runtime_path(rf->tx_ring_map, resolved, sizeof(resolved)) == 0 && resolved[0] != '\0') {
    (void)snprintf(out->ring_path, sizeof(out->ring_path), "%s", resolved);
  } else {
    out->ring_path[0] = '\0';
  }
}

static void mini_gnb_c_fill_common_trx_config(mini_gnb_c_b210_trx_config_t* out,
                                              const char* app_name,
                                              const mini_gnb_c_rf_config_t* rf) {
  char resolved[MINI_GNB_C_MAX_PATH];

  mini_gnb_c_b210_trx_config_init(out);
  (void)snprintf(out->device_args, sizeof(out->device_args), "%s", rf->device_args);
  (void)snprintf(out->subdev, sizeof(out->subdev), "%s", rf->subdev);
  (void)snprintf(out->ref, sizeof(out->ref), "%s", rf->clock_src);
  out->rate_sps = rf->srate;
  out->rx_freq_hz = rf->rx_freq_hz;
  out->tx_freq_hz = rf->tx_freq_hz;
  out->rx_gain_db = rf->rx_gain;
  out->tx_gain_db = rf->tx_gain;
  out->bandwidth_hz = rf->bandwidth_hz;
  out->duration_sec = rf->duration_sec;
  out->duration_mode = mini_gnb_c_b210_duration_mode_from_config(rf->duration_mode);
  out->channel = rf->channel;
  out->channel_count = rf->channel_count;
  out->rx_cpu_core = rf->rx_cpu_core;
  out->tx_cpu_core = rf->tx_cpu_core;
  out->apply_host_tuning = rf->apply_host_tuning;
  out->require_ref_lock = rf->require_ref_lock;
  out->require_lo_lock = rf->require_lo_lock;
  out->ring_block_samples = rf->ring_block_samples;
  out->ring_block_count = rf->ring_block_count;
  out->tx_prefetch_samples = rf->tx_prefetch_samples;

  if (mini_gnb_c_resolve_runtime_path(rf->rx_output_file, resolved, sizeof(resolved)) == 0 && resolved[0] != '\0') {
    (void)snprintf(out->rx_output_path, sizeof(out->rx_output_path), "%s", resolved);
  } else if (mini_gnb_c_build_default_shm_path(app_name, "trx_rx_fc32.dat", out->rx_output_path,
                                               sizeof(out->rx_output_path)) != 0) {
    out->rx_output_path[0] = '\0';
  }
  if (mini_gnb_c_resolve_runtime_path(rf->tx_input_file, resolved, sizeof(resolved)) == 0 && resolved[0] != '\0') {
    (void)snprintf(out->tx_input_path, sizeof(out->tx_input_path), "%s", resolved);
  } else {
    out->tx_input_path[0] = '\0';
  }
  if (mini_gnb_c_resolve_runtime_path(rf->rx_ring_map, resolved, sizeof(resolved)) == 0 && resolved[0] != '\0') {
    (void)snprintf(out->rx_ring_path, sizeof(out->rx_ring_path), "%s", resolved);
  } else if (out->rx_output_path[0] == '\0' &&
             mini_gnb_c_build_default_shm_path(app_name, "trx_rx_ring.map", out->rx_ring_path,
                                               sizeof(out->rx_ring_path)) != 0) {
    out->rx_ring_path[0] = '\0';
  }
  if (mini_gnb_c_resolve_runtime_path(rf->tx_ring_map, resolved, sizeof(resolved)) == 0 && resolved[0] != '\0') {
    (void)snprintf(out->tx_ring_path, sizeof(out->tx_ring_path), "%s", resolved);
  } else {
    out->tx_ring_path[0] = '\0';
  }
}

static bool mini_gnb_c_b210_tx_source_configured(const char* input_path, const char* ring_path) {
  return (input_path != NULL && input_path[0] != '\0') || (ring_path != NULL && ring_path[0] != '\0');
}

static void mini_gnb_c_print_rx_report(const char* app_name, const mini_gnb_c_b210_probe_report_t* report) {
  printf("%s B210 runtime finished.\n", app_name);
  printf("  mode=rx\n");
  printf("  output_kind=%s\n", report->used_ring_map ? "ring-map(sc16)" : "raw-fc32");
  printf("  requested_samples=%zu\n", report->requested_samples);
  printf("  received_samples=%zu\n", report->received_samples);
  printf("  duration_mode=%s\n", report->duration_mode == MINI_GNB_C_B210_DURATION_MODE_WALLCLOCK ? "wallclock" : "samples");
  printf("  wall_elapsed_sec=%.6f\n", report->wall_elapsed_sec);
  printf("  rx_overflow_events=%zu\n", report->rx_overflow_events);
  printf("  rx_timeout_events=%zu\n", report->rx_timeout_events);
  printf("  rx_gap_events=%zu\n", report->rx_gap_events);
  printf("  rx_lost_samples_estimate=%zu\n", report->rx_lost_samples_estimate);
  printf("  ring_blocks=%zu\n", report->ring_blocks_committed);
  printf("  actual_rate_sps=%.0f\n", report->actual_rate_sps);
  printf("  actual_freq_hz=%.0f\n", report->actual_freq_hz);
  printf("  actual_gain_db=%.1f\n", report->actual_gain_db);
}

static void mini_gnb_c_print_tx_report(const char* app_name, const mini_gnb_c_b210_tx_report_t* report) {
  printf("%s B210 runtime finished.\n", app_name);
  printf("  mode=tx\n");
  printf("  input_kind=%s\n", report->used_ring_map ? "ring-map(sc16)" : "raw-fc32");
  printf("  requested_samples=%zu\n", report->requested_samples);
  printf("  transmitted_samples=%zu\n", report->transmitted_samples);
  printf("  ring_blocks=%zu\n", report->ring_blocks_committed);
  printf("  tx_prefetch_samples=%zu\n", report->tx_prefetch_samples);
  printf("  actual_rate_sps=%.0f\n", report->actual_rate_sps);
  printf("  actual_freq_hz=%.0f\n", report->actual_freq_hz);
  printf("  actual_gain_db=%.1f\n", report->actual_gain_db);
  printf("  underflow_observed=%s\n", report->underflow_observed ? "true" : "false");
}

static void mini_gnb_c_print_trx_report(const char* app_name, const mini_gnb_c_b210_trx_report_t* report) {
  printf("%s B210 runtime finished.\n", app_name);
  printf("  mode=trx\n");
  printf("  requested_samples=%zu\n", report->requested_samples);
  printf("  received_samples=%zu\n", report->received_samples);
  printf("  transmitted_samples=%zu\n", report->transmitted_samples);
  printf("  duration_mode=%s\n", report->duration_mode == MINI_GNB_C_B210_DURATION_MODE_WALLCLOCK ? "wallclock" : "samples");
  printf("  wall_elapsed_sec=%.6f\n", report->wall_elapsed_sec);
  printf("  rx_overflow_events=%zu\n", report->rx_overflow_events);
  printf("  rx_timeout_events=%zu\n", report->rx_timeout_events);
  printf("  rx_gap_events=%zu\n", report->rx_gap_events);
  printf("  rx_lost_samples_estimate=%zu\n", report->rx_lost_samples_estimate);
  printf("  tx_ring_wrap_count=%zu\n", report->tx_ring_wrap_count);
  printf("  tx_prefetch_samples=%zu\n", report->tx_prefetch_samples);
  printf("  actual_rate_sps=%.0f\n", report->actual_rate_sps);
  printf("  actual_rx_freq_hz=%.0f\n", report->actual_rx_freq_hz);
  printf("  actual_tx_freq_hz=%.0f\n", report->actual_tx_freq_hz);
  printf("  underflow_observed=%s\n", report->underflow_observed ? "true" : "false");
}

int mini_gnb_c_b210_app_runtime_run(const char* app_name,
                                    const mini_gnb_c_config_t* config,
                                    char* error_message,
                                    const size_t error_message_size) {
  mini_gnb_c_b210_probe_report_t rx_report;
  mini_gnb_c_b210_tx_report_t tx_report;
  mini_gnb_c_b210_trx_report_t trx_report;
  mini_gnb_c_b210_probe_config_t rx_config;
  mini_gnb_c_b210_tx_config_t tx_config;
  mini_gnb_c_b210_trx_config_t trx_config;

  if (app_name == NULL || config == NULL) {
    return mini_gnb_c_fail_b210_runtime(error_message, error_message_size, "invalid B210 app runtime input");
  }
  if (!mini_gnb_c_driver_is_b210(config->rf.device_driver)) {
    return mini_gnb_c_fail_b210_runtime(error_message,
                                        error_message_size,
                                        "rf.runtime_mode requires rf.device_driver set to b210/uhd/uhd-b210");
  }

  switch (config->rf.runtime_mode) {
    case MINI_GNB_C_RF_RUNTIME_MODE_RX:
      memset(&rx_report, 0, sizeof(rx_report));
      mini_gnb_c_fill_common_rx_config(&rx_config, app_name, &config->rf);
      if (mini_gnb_c_b210_probe_run(&rx_config, &rx_report, error_message, error_message_size) != 0) {
        return -1;
      }
      mini_gnb_c_print_rx_report(app_name, &rx_report);
      return 0;
    case MINI_GNB_C_RF_RUNTIME_MODE_TX:
      memset(&tx_report, 0, sizeof(tx_report));
      mini_gnb_c_fill_common_tx_config(&tx_config, &config->rf);
      if (!mini_gnb_c_b210_tx_source_configured(tx_config.input_path, tx_config.ring_path)) {
        return mini_gnb_c_fail_b210_runtime(error_message,
                                            error_message_size,
                                            "rf.runtime_mode=tx requires rf.tx_input_file or rf.tx_ring_map");
      }
      if (mini_gnb_c_b210_tx_from_file_run(&tx_config, &tx_report, error_message, error_message_size) != 0) {
        return -1;
      }
      mini_gnb_c_print_tx_report(app_name, &tx_report);
      return 0;
    case MINI_GNB_C_RF_RUNTIME_MODE_TRX:
      memset(&trx_report, 0, sizeof(trx_report));
      mini_gnb_c_fill_common_trx_config(&trx_config, app_name, &config->rf);
      if (!mini_gnb_c_b210_tx_source_configured(trx_config.tx_input_path, trx_config.tx_ring_path)) {
        return mini_gnb_c_fail_b210_runtime(error_message,
                                            error_message_size,
                                            "rf.runtime_mode=trx requires rf.tx_input_file or rf.tx_ring_map");
      }
      if (mini_gnb_c_b210_trx_run(&trx_config, &trx_report, error_message, error_message_size) != 0) {
        return -1;
      }
      mini_gnb_c_print_trx_report(app_name, &trx_report);
      return 0;
    case MINI_GNB_C_RF_RUNTIME_MODE_SIMULATOR:
      break;
  }

  return mini_gnb_c_fail_b210_runtime(error_message, error_message_size, "unexpected rf.runtime_mode");
}

#else

int mini_gnb_c_b210_app_runtime_run(const char* app_name,
                                    const mini_gnb_c_config_t* config,
                                    char* error_message,
                                    const size_t error_message_size) {
  (void)app_name;
  (void)config;
  return mini_gnb_c_fail_b210_runtime(error_message,
                                      error_message_size,
                                      "this build does not include UHD; rebuild with UHD installed to use rf.runtime_mode");
}

#endif
