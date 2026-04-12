#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mini_gnb_c/radio/sc16_ring_map.h"

static void mini_gnb_c_print_ring_inspect_help(const char* program) {
  fprintf(stderr,
          "Usage: %s [options] <ring.map>\n"
          "\n"
          "Inspect a single-file sc16 ring-map produced by the B210 probe.\n"
          "\n"
          "Options:\n"
          "  --show-blocks <count>  Print up to the latest N ready descriptors, default 8\n"
          "  --help                 Print this message\n",
          program);
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

static const char* mini_gnb_c_ring_time_source_string(const uint32_t flags) {
  if ((flags & MINI_GNB_C_SC16_RING_FLAG_HW_TIME_VALID) != 0u) {
    return "uhd_hw";
  }
  if ((flags & MINI_GNB_C_SC16_RING_FLAG_HOST_TIME_FALLBACK) != 0u) {
    return "host_fallback";
  }
  return "unspecified";
}

int main(int argc, char** argv) {
  mini_gnb_c_sc16_ring_map_t ring;
  const mini_gnb_c_sc16_ring_map_superblock_t* sb = NULL;
  char error_message[256];
  const char* path = NULL;
  long show_blocks = 8;
  int option_index = 0;
  int option = 0;

  static const struct option long_options[] = {
      {"show-blocks", required_argument, NULL, 'n'},
      {"help", no_argument, NULL, 'h'},
      {0, 0, 0, 0},
  };

  memset(&ring, 0, sizeof(ring));
  ring.fd = -1;

  while ((option = getopt_long(argc, argv, "n:h", long_options, &option_index)) != -1) {
    switch (option) {
      case 'n':
        if (mini_gnb_c_parse_long_option(optarg, &show_blocks) != 0 || show_blocks < 0) {
          fprintf(stderr, "invalid --show-blocks value: %s\n", optarg);
          return 1;
        }
        break;
      case 'h':
        mini_gnb_c_print_ring_inspect_help(argv[0]);
        return 0;
      default:
        mini_gnb_c_print_ring_inspect_help(argv[0]);
        return 1;
    }
  }

  if (optind >= argc) {
    mini_gnb_c_print_ring_inspect_help(argv[0]);
    return 1;
  }
  path = argv[optind];
  if (mini_gnb_c_sc16_ring_map_open_existing(path, false, &ring, error_message, sizeof(error_message)) != 0) {
    fprintf(stderr, "mini_ring_inspect failed: %s\n", error_message);
    return 1;
  }
  sb = ring.superblock;

  printf("path=%s\n", path);
  printf("role=%s\n", sb->role == MINI_GNB_C_SC16_RING_ROLE_TX ? "tx" : "rx");
  printf("sample_format=sc16\n");
  printf("sample_rate_sps=%" PRIu64 "\n", sb->sample_rate_sps);
  printf("channel_count=%u\n", sb->channel_count);
  printf("block_count=%u\n", sb->block_count);
  printf("block_samples=%u\n", sb->block_samples);
  printf("block_bytes=%u\n", sb->block_bytes);
  printf("mapped_size=%" PRIu64 "\n", sb->mapped_size);
  printf("oldest_valid_seq=%" PRIu64 "\n", sb->oldest_valid_seq);
  printf("next_write_seq=%" PRIu64 "\n", sb->next_write_seq);
  printf("last_committed_seq=%" PRIu64 "\n", sb->last_committed_seq);
  printf("dropped_blocks=%" PRIu64 "\n", sb->dropped_blocks);
  printf("oldest_slot=%zu\n", mini_gnb_c_sc16_ring_map_slot_for_seq(&ring, sb->oldest_valid_seq));
  printf("next_write_slot=%zu\n", mini_gnb_c_sc16_ring_map_slot_for_seq(&ring, sb->next_write_seq));

  if (show_blocks > 0 && sb->last_committed_seq != UINT64_MAX && sb->next_write_seq > sb->oldest_valid_seq) {
    uint64_t ready_count = sb->next_write_seq - sb->oldest_valid_seq;
    uint64_t start_seq = sb->oldest_valid_seq;
    uint64_t max_blocks = (uint64_t)show_blocks;
    uint64_t seq = 0u;

    if (ready_count > max_blocks) {
      start_seq = sb->next_write_seq - max_blocks;
    }
    printf("descriptors:\n");
    for (seq = start_seq; seq < sb->next_write_seq; ++seq) {
      const mini_gnb_c_sc16_ring_map_descriptor_t* descriptor = mini_gnb_c_sc16_ring_map_get_descriptor(&ring, seq);

      if (descriptor == NULL) {
        continue;
      }
      printf("  seq=%" PRIu64 " slot=%zu sample_count=%u payload_bytes=%u hw_time_ns=%" PRIu64 " flags=0x%x time_source=%s\n",
             descriptor->seqno,
             mini_gnb_c_sc16_ring_map_slot_for_seq(&ring, seq),
             descriptor->sample_count,
             descriptor->payload_bytes,
             descriptor->hw_time_ns,
             descriptor->flags,
             mini_gnb_c_ring_time_source_string(descriptor->flags));
    }
  }

  mini_gnb_c_sc16_ring_map_close(&ring);
  return 0;
}
