#ifndef MINI_GNB_C_RADIO_SC16_RING_EXPORT_H
#define MINI_GNB_C_RADIO_SC16_RING_EXPORT_H

#include <stddef.h>
#include <stdint.h>

#include "mini_gnb_c/radio/sc16_ring_map.h"

typedef struct {
  uint64_t seq_start;
  uint64_t seq_end;
  uint64_t blocks_exported;
  uint64_t samples_per_channel;
  uint32_t channel_count;
  uint64_t sample_rate_sps;
} mini_gnb_c_sc16_ring_export_report_t;

int mini_gnb_c_sc16_ring_export_range(const mini_gnb_c_sc16_ring_map_t* ring,
                                      uint64_t seq_start,
                                      uint64_t seq_end,
                                      const char* output_prefix,
                                      mini_gnb_c_sc16_ring_export_report_t* report,
                                      char* error_message,
                                      size_t error_message_size);

#endif
