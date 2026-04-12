#include <stdio.h>
#include <string.h>

#include "test_helpers.h"

#include "mini_gnb_c/radio/sc16_ring_map.h"

void test_sc16_ring_map_create_append_and_wrap(void) {
  mini_gnb_c_sc16_ring_map_config_t config;
  mini_gnb_c_sc16_ring_map_t ring;
  mini_gnb_c_sc16_ring_map_t reopened;
  char out_dir[MINI_GNB_C_MAX_PATH];
  char ring_path[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  int16_t block0[8 * 2];
  int16_t block1[8 * 2];
  int16_t block2[8 * 2];
  int16_t block3[8 * 2];
  int16_t block4[8 * 2];
  const mini_gnb_c_sc16_ring_map_descriptor_t* descriptor = NULL;
  const int16_t* payload = NULL;
  size_t i = 0u;

  mini_gnb_c_sc16_ring_map_config_init(&config);
  memset(&ring, 0, sizeof(ring));
  memset(&reopened, 0, sizeof(reopened));
  ring.fd = -1;
  reopened.fd = -1;
  config.role = MINI_GNB_C_SC16_RING_ROLE_RX;
  config.block_count = 4u;
  config.block_samples = 8u;
  config.sample_rate_sps = 30720000u;

  for (i = 0u; i < 16u; ++i) {
    block0[i] = (int16_t)i;
    block1[i] = (int16_t)(100 + i);
    block2[i] = (int16_t)(200 + i);
    block3[i] = (int16_t)(300 + i);
    block4[i] = (int16_t)(400 + i);
  }

  mini_gnb_c_make_output_dir("test_sc16_ring_map", out_dir, sizeof(out_dir));
  mini_gnb_c_require((size_t)snprintf(ring_path, sizeof(ring_path), "%s/rx_ring.map", out_dir) < sizeof(ring_path),
                     "expected ring map path to fit");

  mini_gnb_c_require(mini_gnb_c_sc16_ring_map_create(ring_path, &config, &ring, error_message, sizeof(error_message)) == 0,
                     error_message);
  mini_gnb_c_require(mini_gnb_c_sc16_ring_map_append(&ring, block0, 8u, 1000u, 0x1u, error_message, sizeof(error_message)) == 0,
                     error_message);
  mini_gnb_c_require(mini_gnb_c_sc16_ring_map_append(&ring, block1, 6u, 2000u, 0x2u, error_message, sizeof(error_message)) == 0,
                     error_message);
  mini_gnb_c_require(mini_gnb_c_sc16_ring_map_append(&ring, block2, 5u, 3000u, 0x3u, error_message, sizeof(error_message)) == 0,
                     error_message);
  mini_gnb_c_require(mini_gnb_c_sc16_ring_map_append(&ring, block3, 7u, 4000u, 0x4u, error_message, sizeof(error_message)) == 0,
                     error_message);

  mini_gnb_c_require(ring.superblock->oldest_valid_seq == 0u, "expected oldest sequence 0 before wrap");
  mini_gnb_c_require(ring.superblock->next_write_seq == 4u, "expected next write sequence 4 before wrap");
  mini_gnb_c_require(ring.superblock->last_committed_seq == 3u, "expected last committed sequence 3 before wrap");
  mini_gnb_c_require(ring.superblock->dropped_blocks == 0u, "expected no dropped blocks before wrap");

  descriptor = mini_gnb_c_sc16_ring_map_get_descriptor(&ring, 2u);
  mini_gnb_c_require(descriptor != NULL, "expected descriptor for sequence 2");
  mini_gnb_c_require(descriptor->sample_count == 5u, "expected sequence 2 sample count");
  mini_gnb_c_require(descriptor->flags == 0x3u, "expected sequence 2 flags");
  payload = mini_gnb_c_sc16_ring_map_get_payload(&ring, 2u);
  mini_gnb_c_require(payload != NULL, "expected payload for sequence 2");
  mini_gnb_c_require(payload[0] == 200 && payload[1] == 201, "expected sequence 2 payload samples");

  mini_gnb_c_require(mini_gnb_c_sc16_ring_map_append(&ring, block4, 4u, 5000u, 0x5u, error_message, sizeof(error_message)) == 0,
                     error_message);
  mini_gnb_c_require(ring.superblock->oldest_valid_seq == 1u, "expected oldest sequence to advance after wrap");
  mini_gnb_c_require(ring.superblock->next_write_seq == 5u, "expected next write sequence 5 after wrap");
  mini_gnb_c_require(ring.superblock->last_committed_seq == 4u, "expected last committed sequence 4 after wrap");
  mini_gnb_c_require(ring.superblock->dropped_blocks == 1u, "expected one dropped block after wrap");
  mini_gnb_c_require(!mini_gnb_c_sc16_ring_map_seq_valid(&ring, 0u), "expected wrapped-out sequence to become invalid");
  mini_gnb_c_require(mini_gnb_c_sc16_ring_map_seq_valid(&ring, 4u), "expected latest sequence to be valid");

  mini_gnb_c_sc16_ring_map_close(&ring);

  mini_gnb_c_require(mini_gnb_c_sc16_ring_map_open_existing(ring_path, false, &reopened, error_message, sizeof(error_message)) == 0,
                     error_message);
  mini_gnb_c_require(reopened.superblock->block_count == 4u, "expected reopened block count");
  mini_gnb_c_require(reopened.superblock->block_samples == 8u, "expected reopened block samples");
  mini_gnb_c_require(reopened.superblock->channel_count == 1u, "expected reopened single-channel ring");
  mini_gnb_c_require(reopened.superblock->sample_rate_sps == 30720000u, "expected reopened sample rate");
  descriptor = mini_gnb_c_sc16_ring_map_get_descriptor(&reopened, 4u);
  mini_gnb_c_require(descriptor != NULL, "expected reopened descriptor for latest sequence");
  mini_gnb_c_require(descriptor->sample_count == 4u, "expected reopened latest sequence sample count");
  payload = mini_gnb_c_sc16_ring_map_get_payload(&reopened, 4u);
  mini_gnb_c_require(payload != NULL, "expected reopened payload for latest sequence");
  mini_gnb_c_require(payload[0] == 400 && payload[1] == 401, "expected reopened payload samples");

  mini_gnb_c_sc16_ring_map_close(&reopened);
}

void test_sc16_ring_map_dual_channel_payload_is_channel_major(void) {
  mini_gnb_c_sc16_ring_map_config_t config;
  mini_gnb_c_sc16_ring_map_t ring;
  char out_dir[MINI_GNB_C_MAX_PATH];
  char ring_path[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  int16_t block[(4 * 2) * 2];
  const mini_gnb_c_sc16_ring_map_descriptor_t* descriptor = NULL;
  const int16_t* channel0 = NULL;
  const int16_t* channel1 = NULL;
  size_t i = 0u;

  mini_gnb_c_sc16_ring_map_config_init(&config);
  memset(&ring, 0, sizeof(ring));
  ring.fd = -1;
  config.role = MINI_GNB_C_SC16_RING_ROLE_RX;
  config.channel_count = 2u;
  config.block_count = 8u;
  config.block_samples = 4u;
  config.sample_rate_sps = 20000000u;

  for (i = 0u; i < 8u; ++i) {
    block[i] = (int16_t)(10 + i);
    block[8u + i] = (int16_t)(110 + i);
  }

  mini_gnb_c_make_output_dir("test_sc16_ring_map_dual", out_dir, sizeof(out_dir));
  mini_gnb_c_require((size_t)snprintf(ring_path, sizeof(ring_path), "%s/rx_ring_dual.map", out_dir) < sizeof(ring_path),
                     "expected dual ring map path to fit");
  mini_gnb_c_require(mini_gnb_c_sc16_ring_map_create(ring_path, &config, &ring, error_message, sizeof(error_message)) == 0,
                     error_message);
  mini_gnb_c_require(mini_gnb_c_sc16_ring_map_append(&ring, block, 4u, 1234u, 0u, error_message, sizeof(error_message)) == 0,
                     error_message);

  descriptor = mini_gnb_c_sc16_ring_map_get_descriptor(&ring, 0u);
  mini_gnb_c_require(descriptor != NULL, "expected descriptor for dual-channel block");
  mini_gnb_c_require(descriptor->payload_bytes == 32u, "expected dual-channel payload byte count");
  channel0 = mini_gnb_c_sc16_ring_map_get_channel_payload(&ring, 0u, 0u);
  channel1 = mini_gnb_c_sc16_ring_map_get_channel_payload(&ring, 0u, 1u);
  mini_gnb_c_require(channel0 != NULL && channel1 != NULL, "expected both channel payload pointers");
  mini_gnb_c_require(channel0[0] == 10 && channel0[1] == 11, "expected channel 0 payload at block head");
  mini_gnb_c_require(channel1[0] == 110 && channel1[1] == 111, "expected channel 1 payload after channel 0 region");

  mini_gnb_c_sc16_ring_map_close(&ring);
}
