#include <stdio.h>
#include <string.h>

#include "test_helpers.h"

#include "mini_gnb_c/radio/sc16_ring_export.h"
#include "mini_gnb_c/radio/sc16_ring_map.h"

void test_sc16_ring_export_range_writes_per_channel_files(void) {
  mini_gnb_c_sc16_ring_map_config_t config;
  mini_gnb_c_sc16_ring_map_t ring;
  mini_gnb_c_sc16_ring_map_t reopened;
  mini_gnb_c_sc16_ring_export_report_t report;
  char out_dir[MINI_GNB_C_MAX_PATH];
  char ring_path[MINI_GNB_C_MAX_PATH];
  char export_prefix[MINI_GNB_C_MAX_PATH];
  char ch0_path[MINI_GNB_C_MAX_PATH];
  char ch1_path[MINI_GNB_C_MAX_PATH];
  char meta_path[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  char metadata_text[512];
  int16_t block0[16];
  int16_t block1[16];
  int16_t block2[16];
  size_t i = 0u;
  FILE* fp = NULL;
  size_t bytes_read = 0u;

  mini_gnb_c_sc16_ring_map_config_init(&config);
  memset(&ring, 0, sizeof(ring));
  memset(&reopened, 0, sizeof(reopened));
  memset(&report, 0, sizeof(report));
  ring.fd = -1;
  reopened.fd = -1;
  config.role = MINI_GNB_C_SC16_RING_ROLE_RX;
  config.channel_count = 2u;
  config.block_count = 8u;
  config.block_samples = 4u;
  config.sample_rate_sps = 30720000u;

  for (i = 0u; i < 16u; ++i) {
    block0[i] = (int16_t)(10 + i);
    block1[i] = (int16_t)(110 + i);
    block2[i] = (int16_t)(210 + i);
  }

  mini_gnb_c_make_output_dir("test_sc16_ring_export", out_dir, sizeof(out_dir));
  mini_gnb_c_require((size_t)snprintf(ring_path, sizeof(ring_path), "%s/rx_ring.map", out_dir) < sizeof(ring_path),
                     "expected export ring path to fit");
  mini_gnb_c_require((size_t)snprintf(export_prefix, sizeof(export_prefix), "%s/export/rx_slice", out_dir) < sizeof(export_prefix),
                     "expected export prefix to fit");
  mini_gnb_c_require(mini_gnb_c_sc16_ring_map_create(ring_path, &config, &ring, error_message, sizeof(error_message)) == 0,
                     error_message);
  mini_gnb_c_require(mini_gnb_c_sc16_ring_map_append(&ring, block0, 4u, 100u, 0u, error_message, sizeof(error_message)) == 0,
                     error_message);
  mini_gnb_c_require(mini_gnb_c_sc16_ring_map_append(&ring, block1, 3u, 200u, 0u, error_message, sizeof(error_message)) == 0,
                     error_message);
  mini_gnb_c_require(mini_gnb_c_sc16_ring_map_append(&ring, block2, 2u, 300u, 0u, error_message, sizeof(error_message)) == 0,
                     error_message);
  mini_gnb_c_sc16_ring_map_close(&ring);

  mini_gnb_c_require(mini_gnb_c_sc16_ring_map_open_existing(ring_path, false, &reopened, error_message, sizeof(error_message)) == 0,
                     error_message);
  mini_gnb_c_require(mini_gnb_c_sc16_ring_export_range(&reopened,
                                                       1u,
                                                       2u,
                                                       export_prefix,
                                                       &report,
                                                       error_message,
                                                       sizeof(error_message)) == 0,
                     error_message);
  mini_gnb_c_require(report.blocks_exported == 2u, "expected two exported blocks");
  mini_gnb_c_require(report.samples_per_channel == 5u, "expected exported sample sum per channel");
  mini_gnb_c_require(report.channel_count == 2u, "expected dual-channel export");

  mini_gnb_c_require((size_t)snprintf(ch0_path, sizeof(ch0_path), "%s_ch0.sc16", export_prefix) < sizeof(ch0_path),
                     "expected ch0 export path to fit");
  mini_gnb_c_require((size_t)snprintf(ch1_path, sizeof(ch1_path), "%s_ch1.sc16", export_prefix) < sizeof(ch1_path),
                     "expected ch1 export path to fit");
  mini_gnb_c_require((size_t)snprintf(meta_path, sizeof(meta_path), "%s_meta.txt", export_prefix) < sizeof(meta_path),
                     "expected meta export path to fit");
  mini_gnb_c_require(mini_gnb_c_path_exists(ch0_path), "expected channel 0 export file");
  mini_gnb_c_require(mini_gnb_c_path_exists(ch1_path), "expected channel 1 export file");
  mini_gnb_c_require(mini_gnb_c_path_exists(meta_path), "expected export metadata file");
  mini_gnb_c_require(mini_gnb_c_file_size(ch0_path) == 5u * sizeof(int16_t) * 2u, "expected channel 0 export size");
  mini_gnb_c_require(mini_gnb_c_file_size(ch1_path) == 5u * sizeof(int16_t) * 2u, "expected channel 1 export size");

  fp = fopen(meta_path, "rb");
  mini_gnb_c_require(fp != NULL, "expected metadata file to open");
  bytes_read = fread(metadata_text, 1u, sizeof(metadata_text) - 1u, fp);
  metadata_text[bytes_read] = '\0';
  fclose(fp);
  fp = NULL;
  mini_gnb_c_require(strstr(metadata_text, "seq_start=1") != NULL, "expected metadata seq_start");
  mini_gnb_c_require(strstr(metadata_text, "seq_end=2") != NULL, "expected metadata seq_end");
  mini_gnb_c_require(strstr(metadata_text, "channel_0_file=") != NULL, "expected metadata channel 0 path");
  mini_gnb_c_require(strstr(metadata_text, "channel_1_file=") != NULL, "expected metadata channel 1 path");

  mini_gnb_c_sc16_ring_map_close(&reopened);
}
