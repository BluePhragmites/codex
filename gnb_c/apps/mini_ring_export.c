#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mini_gnb_c/radio/sc16_ring_export.h"
#include "mini_gnb_c/radio/sc16_ring_map.h"

static void mini_gnb_c_print_ring_export_help(const char* program) {
  fprintf(stderr,
          "Usage: %s --seq-start <n> --seq-end <n> --output-prefix <path> <ring.map>\n"
          "\n"
          "Export a seq range from a single-file sc16 ring map.\n"
          "\n"
          "Output files:\n"
          "  <prefix>_ch0.sc16\n"
          "  <prefix>_ch1.sc16    when channel_count=2\n"
          "  <prefix>_meta.txt\n"
          "\n"
          "Options:\n"
          "  --seq-start <n>      Inclusive start seq\n"
          "  --seq-end <n>        Inclusive end seq\n"
          "  --output-prefix <p>  Output file prefix\n"
          "  --help               Print this message\n",
          program);
}

static int mini_gnb_c_parse_u64_option(const char* text, uint64_t* out) {
  char* end = NULL;

  if (text == NULL || out == NULL) {
    return -1;
  }
  *out = strtoull(text, &end, 10);
  if (end == text || *end != '\0') {
    return -1;
  }
  return 0;
}

int main(int argc, char** argv) {
  mini_gnb_c_sc16_ring_map_t ring;
  mini_gnb_c_sc16_ring_export_report_t report;
  char error_message[256];
  const char* path = NULL;
  char output_prefix[MINI_GNB_C_MAX_PATH];
  uint64_t seq_start = 0u;
  uint64_t seq_end = 0u;
  bool have_seq_start = false;
  bool have_seq_end = false;
  bool have_output_prefix = false;
  int option_index = 0;
  int option = 0;

  static const struct option long_options[] = {
      {"seq-start", required_argument, NULL, 's'},
      {"seq-end", required_argument, NULL, 'e'},
      {"output-prefix", required_argument, NULL, 'o'},
      {"help", no_argument, NULL, 'h'},
      {0, 0, 0, 0},
  };

  memset(&ring, 0, sizeof(ring));
  memset(&report, 0, sizeof(report));
  memset(output_prefix, 0, sizeof(output_prefix));
  ring.fd = -1;

  while ((option = getopt_long(argc, argv, "s:e:o:h", long_options, &option_index)) != -1) {
    switch (option) {
      case 's':
        if (mini_gnb_c_parse_u64_option(optarg, &seq_start) != 0) {
          fprintf(stderr, "invalid --seq-start value: %s\n", optarg);
          return 1;
        }
        have_seq_start = true;
        break;
      case 'e':
        if (mini_gnb_c_parse_u64_option(optarg, &seq_end) != 0) {
          fprintf(stderr, "invalid --seq-end value: %s\n", optarg);
          return 1;
        }
        have_seq_end = true;
        break;
      case 'o':
        if ((size_t)snprintf(output_prefix, sizeof(output_prefix), "%s", optarg) >= sizeof(output_prefix)) {
          fprintf(stderr, "output prefix is too long\n");
          return 1;
        }
        have_output_prefix = true;
        break;
      case 'h':
        mini_gnb_c_print_ring_export_help(argv[0]);
        return 0;
      default:
        mini_gnb_c_print_ring_export_help(argv[0]);
        return 1;
    }
  }

  if (!have_seq_start || !have_seq_end || !have_output_prefix || optind >= argc) {
    mini_gnb_c_print_ring_export_help(argv[0]);
    return 1;
  }
  path = argv[optind];
  if (mini_gnb_c_sc16_ring_map_open_existing(path, false, &ring, error_message, sizeof(error_message)) != 0) {
    fprintf(stderr, "mini_ring_export failed: %s\n", error_message);
    return 1;
  }
  if (mini_gnb_c_sc16_ring_export_range(&ring,
                                        seq_start,
                                        seq_end,
                                        output_prefix,
                                        &report,
                                        error_message,
                                        sizeof(error_message)) != 0) {
    mini_gnb_c_sc16_ring_map_close(&ring);
    fprintf(stderr, "mini_ring_export failed: %s\n", error_message);
    return 1;
  }

  printf("mini_ring_export completed successfully\n");
  printf("  ring=%s\n", path);
  printf("  seq_start=%" PRIu64 "\n", report.seq_start);
  printf("  seq_end=%" PRIu64 "\n", report.seq_end);
  printf("  blocks_exported=%" PRIu64 "\n", report.blocks_exported);
  printf("  samples_per_channel=%" PRIu64 "\n", report.samples_per_channel);
  printf("  channel_count=%u\n", report.channel_count);
  printf("  sample_rate_sps=%" PRIu64 "\n", report.sample_rate_sps);
  printf("  output_prefix=%s\n", output_prefix);

  mini_gnb_c_sc16_ring_map_close(&ring);
  return 0;
}
