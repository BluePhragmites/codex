#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_helpers.h"

#include "mini_gnb_c/common/hex.h"
#include "mini_gnb_c/common/json_utils.h"
#include "mini_gnb_c/common/simulator.h"
#include "mini_gnb_c/config/config_loader.h"

void test_integration_run(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char output_dir[MINI_GNB_C_MAX_PATH];
  char iq_cf32_name[96];
  char iq_json_name[96];
  char iq_dir[MINI_GNB_C_MAX_PATH];
  char iq_cf32_path[MINI_GNB_C_MAX_PATH];
  char iq_json_path[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  char ra_contention_id_hex[MINI_GNB_C_MAX_TEXT];
  char ue_contention_id_hex[MINI_GNB_C_MAX_TEXT];
  mini_gnb_c_config_t config;
  mini_gnb_c_simulator_t simulator;
  mini_gnb_c_run_summary_t summary;
  char* summary_json = NULL;
  char* iq_json = NULL;
  FILE* iq_file = NULL;

  mini_gnb_c_default_config_path(config_path, sizeof(config_path));
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) == 0,
                     "expected config to load");
  mini_gnb_c_make_output_dir("test_integration_c", output_dir, sizeof(output_dir));
  mini_gnb_c_simulator_init(&simulator, &config, output_dir);
  mini_gnb_c_require(mini_gnb_c_simulator_run(&simulator, &summary) == 0, "expected simulator run");

  mini_gnb_c_require(summary.counters.prach_detect_ok >= 1U, "expected PRACH detection");
  mini_gnb_c_require(summary.counters.rar_sent >= 1U, "expected RAR transmission");
  mini_gnb_c_require(summary.counters.msg3_crc_ok >= 1U, "expected Msg3 CRC success");
  mini_gnb_c_require(summary.counters.rrcsetup_sent >= 1U, "expected Msg4/RRCSetup transmission");
  mini_gnb_c_require(summary.ue_count > 0U, "expected at least one promoted UE context");
  mini_gnb_c_require(summary.ue_contexts[0].rrc_setup_sent, "expected UE context marked after Msg4");
  mini_gnb_c_require(summary.has_ra_context, "expected RA context in summary");
  mini_gnb_c_require(summary.ra_context.has_contention_id, "expected resolved contention identity");
  mini_gnb_c_require(mini_gnb_c_bytes_to_hex(summary.ra_context.contention_id48,
                                             6U,
                                             ra_contention_id_hex,
                                             sizeof(ra_contention_id_hex)) == 0,
                     "expected RA contention identity serialization");
  mini_gnb_c_require(mini_gnb_c_bytes_to_hex(summary.ue_contexts[0].contention_id48,
                                             6U,
                                             ue_contention_id_hex,
                                             sizeof(ue_contention_id_hex)) == 0,
                     "expected UE contention identity serialization");
  mini_gnb_c_require(strcmp(ra_contention_id_hex, ue_contention_id_hex) == 0,
                     "expected RA context and UE context to share the same contention identity");

  summary_json = mini_gnb_c_read_text_file(summary.summary_path);
  mini_gnb_c_require(summary_json != NULL, "expected summary.json to be readable");

  (void)snprintf(iq_cf32_name,
                 sizeof(iq_cf32_name),
                 "slot_7_DL_OBJ_MSG4_rnti_%u.cf32",
                 summary.ue_contexts[0].tc_rnti);
  (void)snprintf(iq_json_name,
                 sizeof(iq_json_name),
                 "slot_7_DL_OBJ_MSG4_rnti_%u.json",
                 summary.ue_contexts[0].tc_rnti);
  mini_gnb_c_require(mini_gnb_c_join_path(output_dir, "iq", iq_dir, sizeof(iq_dir)) == 0,
                     "expected iq directory path");
  mini_gnb_c_require(mini_gnb_c_join_path(iq_dir, iq_cf32_name, iq_cf32_path, sizeof(iq_cf32_path)) == 0,
                     "expected cf32 path");
  mini_gnb_c_require(mini_gnb_c_join_path(iq_dir, iq_json_name, iq_json_path, sizeof(iq_json_path)) == 0,
                     "expected json path");

  iq_file = fopen(iq_cf32_path, "rb");
  mini_gnb_c_require(iq_file != NULL, "expected Msg4 cf32 export");
  fclose(iq_file);

  iq_json = mini_gnb_c_read_text_file(iq_json_path);
  mini_gnb_c_require(iq_json != NULL, "expected Msg4 IQ metadata export");
  mini_gnb_c_require(strstr(iq_json, "\"sample_count\":2016") != NULL,
                     "expected IQ metadata to describe waveform sample count");

  free(iq_json);
  free(summary_json);
}
