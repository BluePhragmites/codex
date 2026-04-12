#ifndef MINI_GNB_C_RADIO_SC16_RING_MAP_H
#define MINI_GNB_C_RADIO_SC16_RING_MAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mini_gnb_c/common/types.h"

#define MINI_GNB_C_SC16_RING_MAP_MAGIC 0x4d474352494e4731ULL
#define MINI_GNB_C_SC16_RING_MAP_VERSION 1u
#define MINI_GNB_C_SC16_RING_FLAG_HW_TIME_VALID 0x1u
#define MINI_GNB_C_SC16_RING_FLAG_HOST_TIME_FALLBACK 0x2u

typedef enum {
  MINI_GNB_C_SC16_RING_ROLE_RX = 0,
  MINI_GNB_C_SC16_RING_ROLE_TX = 1
} mini_gnb_c_sc16_ring_role_t;

typedef enum {
  MINI_GNB_C_SC16_RING_BLOCK_EMPTY = 0,
  MINI_GNB_C_SC16_RING_BLOCK_WRITING = 1,
  MINI_GNB_C_SC16_RING_BLOCK_READY = 2
} mini_gnb_c_sc16_ring_block_state_t;

typedef struct {
  uint64_t magic;
  uint32_t version;
  uint32_t role;
  uint32_t sample_format;
  uint32_t channel_count;
  uint32_t block_count;
  uint32_t block_samples;
  uint32_t block_bytes;
  uint64_t sample_rate_sps;
  uint64_t mapped_size;
  uint64_t descriptor_offset;
  uint64_t payload_offset;
  uint64_t oldest_valid_seq;
  uint64_t next_write_seq;
  uint64_t last_committed_seq;
  uint64_t dropped_blocks;
  uint64_t reserved[4];
} mini_gnb_c_sc16_ring_map_superblock_t;

typedef struct {
  uint64_t seqno;
  uint64_t hw_time_ns;
  uint64_t payload_offset;
  uint32_t sample_count;
  uint32_t payload_bytes;
  uint32_t flags;
  uint32_t state;
  uint64_t reserved[2];
} mini_gnb_c_sc16_ring_map_descriptor_t;

typedef struct {
  mini_gnb_c_sc16_ring_role_t role;
  uint32_t channel_count;
  uint32_t block_count;
  uint32_t block_samples;
  uint64_t sample_rate_sps;
} mini_gnb_c_sc16_ring_map_config_t;

typedef struct {
  int fd;
  size_t mapped_size;
  bool writable;
  void* mapping;
  mini_gnb_c_sc16_ring_map_superblock_t* superblock;
  mini_gnb_c_sc16_ring_map_descriptor_t* descriptors;
  uint8_t* payload_base;
  char path[MINI_GNB_C_MAX_PATH];
} mini_gnb_c_sc16_ring_map_t;

void mini_gnb_c_sc16_ring_map_config_init(mini_gnb_c_sc16_ring_map_config_t* config);

int mini_gnb_c_sc16_ring_map_create(const char* path,
                                    const mini_gnb_c_sc16_ring_map_config_t* config,
                                    mini_gnb_c_sc16_ring_map_t* ring,
                                    char* error_message,
                                    size_t error_message_size);

int mini_gnb_c_sc16_ring_map_open_existing(const char* path,
                                           const bool writable,
                                           mini_gnb_c_sc16_ring_map_t* ring,
                                           char* error_message,
                                           size_t error_message_size);

void mini_gnb_c_sc16_ring_map_close(mini_gnb_c_sc16_ring_map_t* ring);

int mini_gnb_c_sc16_ring_map_append(mini_gnb_c_sc16_ring_map_t* ring,
                                    const int16_t* iq_samples,
                                    uint32_t sample_count,
                                    uint64_t hw_time_ns,
                                    uint32_t flags,
                                    char* error_message,
                                    size_t error_message_size);

bool mini_gnb_c_sc16_ring_map_seq_valid(const mini_gnb_c_sc16_ring_map_t* ring, uint64_t seqno);

size_t mini_gnb_c_sc16_ring_map_slot_for_seq(const mini_gnb_c_sc16_ring_map_t* ring, uint64_t seqno);

const mini_gnb_c_sc16_ring_map_descriptor_t* mini_gnb_c_sc16_ring_map_get_descriptor(
    const mini_gnb_c_sc16_ring_map_t* ring, uint64_t seqno);

const int16_t* mini_gnb_c_sc16_ring_map_get_payload(const mini_gnb_c_sc16_ring_map_t* ring, uint64_t seqno);

const int16_t* mini_gnb_c_sc16_ring_map_get_channel_payload(const mini_gnb_c_sc16_ring_map_t* ring,
                                                            uint64_t seqno,
                                                            uint32_t channel_index);

#endif
