#include <string.h>

#include "test_helpers.h"

#include "mini_gnb_c/config/config_loader.h"
#include "mini_gnb_c/timing/slot_engine.h"

void test_config_loads(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  mini_gnb_c_config_t config;

  mini_gnb_c_default_config_path(config_path, sizeof(config_path));
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) == 0,
                     "expected config to load");
  mini_gnb_c_require(config.cell.pci == 1U, "expected pci=1");
  mini_gnb_c_require(config.cell.band == 78U, "expected band n78");
  mini_gnb_c_require(config.prach.ra_resp_window == 4U, "expected ra response window");
  mini_gnb_c_require(strcmp(config.rf.device_driver, "mock") == 0, "expected mock radio frontend");
  mini_gnb_c_require(!config.core.enabled, "expected core bridge disabled by default");
  mini_gnb_c_require(strcmp(config.core.amf_ip, "127.0.0.5") == 0, "expected default AMF IP");
  mini_gnb_c_require(config.core.amf_port == 38412u, "expected default AMF port");
  mini_gnb_c_require(config.core.timeout_ms == 5000u, "expected default AMF timeout");
  mini_gnb_c_require(config.core.ran_ue_ngap_id_base == 1u, "expected default RAN UE NGAP ID base");
  mini_gnb_c_require(config.core.default_pdu_session_id == 1u, "expected default requested PDU session ID");
  mini_gnb_c_require(config.sim.total_slots == 20, "expected total slot count");
  mini_gnb_c_require(config.sim.prach_retry_delay_slots == 4, "expected PRACH retry delay");
  mini_gnb_c_require(config.sim.msg3_present, "expected Msg3 burst enabled");
  mini_gnb_c_require(strcmp(config.sim.ul_input_dir, "input") == 0, "expected default slot input directory");
  mini_gnb_c_require(strcmp(config.sim.local_exchange_dir, "out/local_exchange") == 0,
                     "expected default local exchange directory");
  mini_gnb_c_require(config.sim.shared_slot_path[0] == '\0', "expected shared slot path disabled by default");
  mini_gnb_c_require(config.sim.shared_slot_timeout_ms == 100u, "expected default shared slot timeout");
  mini_gnb_c_require(config.sim.post_msg4_traffic_enabled, "expected post-Msg4 traffic enabled");
  mini_gnb_c_require(config.sim.post_msg4_dl_pdcch_delay_slots == 1, "expected DL PDCCH delay");
  mini_gnb_c_require(config.sim.post_msg4_dl_time_indicator == 1, "expected DL time indicator");
  mini_gnb_c_require(config.sim.post_msg4_dl_data_to_ul_ack_slots == 1, "expected DL ACK timing");
  mini_gnb_c_require(config.sim.post_msg4_sr_period_slots == 10, "expected SR period");
  mini_gnb_c_require(config.sim.post_msg4_sr_offset_slot == 2, "expected SR offset");
  mini_gnb_c_require(config.sim.post_msg4_ul_grant_delay_slots == 1, "expected UL grant delay");
  mini_gnb_c_require(config.sim.post_msg4_ul_time_indicator == 2, "expected UL time indicator");
  mini_gnb_c_require(config.sim.post_msg4_dl_harq_process_count == 2, "expected DL HARQ process count");
  mini_gnb_c_require(config.sim.post_msg4_ul_harq_process_count == 2, "expected UL HARQ process count");
  mini_gnb_c_require(config.sim.ul_data_present, "expected synthetic UL data enabled");
  mini_gnb_c_require(config.sim.ul_bsr_buffer_size_bytes == 384, "expected default BSR size");
  mini_gnb_c_require(config.sim.include_crnti_ce, "expected simulated C-RNTI CE");
  mini_gnb_c_require(config.broadcast.sib1_period_slots == 8, "expected SIB1 period");
  mini_gnb_c_require(config.broadcast.sib1_offset_slot == 4, "expected SIB1 offset");
  mini_gnb_c_require(config.broadcast.prach_period_slots == 10, "expected PRACH period");
  mini_gnb_c_require(config.broadcast.prach_offset_slot == 2, "expected PRACH offset");
}

void test_sib1_schedule_uses_period_and_offset(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  mini_gnb_c_config_t config;
  mini_gnb_c_slot_engine_t slot_engine;
  mini_gnb_c_slot_indication_t slot0;
  mini_gnb_c_slot_indication_t slot4;
  mini_gnb_c_slot_indication_t slot12;
  mini_gnb_c_slot_indication_t slot20;

  mini_gnb_c_default_config_path(config_path, sizeof(config_path));
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) == 0,
                     "expected config to load");

  mini_gnb_c_slot_engine_init(&slot_engine, &config);
  mini_gnb_c_slot_engine_make_slot(&slot_engine, 0, &slot0);
  mini_gnb_c_slot_engine_make_slot(&slot_engine, 4, &slot4);
  mini_gnb_c_slot_engine_make_slot(&slot_engine, 12, &slot12);
  mini_gnb_c_slot_engine_make_slot(&slot_engine, 20, &slot20);

  mini_gnb_c_require(!slot0.has_sib1, "expected no SIB1 at abs_slot 0 when offset is 4");
  mini_gnb_c_require(slot4.has_sib1, "expected SIB1 at abs_slot 4");
  mini_gnb_c_require(slot12.has_sib1, "expected SIB1 at abs_slot 12");
  mini_gnb_c_require(slot20.has_sib1, "expected SIB1 at abs_slot 20");
}

void test_prach_schedule_uses_period_and_offset(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  mini_gnb_c_config_t config;
  mini_gnb_c_slot_engine_t slot_engine;
  mini_gnb_c_slot_indication_t slot1;
  mini_gnb_c_slot_indication_t slot2;
  mini_gnb_c_slot_indication_t slot12;

  mini_gnb_c_default_config_path(config_path, sizeof(config_path));
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) == 0,
                     "expected config to load");

  mini_gnb_c_slot_engine_init(&slot_engine, &config);
  mini_gnb_c_slot_engine_make_slot(&slot_engine, 1, &slot1);
  mini_gnb_c_slot_engine_make_slot(&slot_engine, 2, &slot2);
  mini_gnb_c_slot_engine_make_slot(&slot_engine, 12, &slot12);

  mini_gnb_c_require(!slot1.has_prach_occasion, "expected no PRACH occasion at abs_slot 1");
  mini_gnb_c_require(slot2.has_prach_occasion, "expected PRACH occasion at abs_slot 2");
  mini_gnb_c_require(slot12.has_prach_occasion, "expected periodic PRACH occasion at abs_slot 12");
}

void test_tbsize_lookup_table(void) {
  mini_gnb_c_require(mini_gnb_c_lookup_tbsize(8U, 4U) == 16U, "expected compact UL BSR tbsize");
  mini_gnb_c_require(mini_gnb_c_lookup_tbsize(24U, 8U) == 96U, "expected large UL payload tbsize");
  mini_gnb_c_require(mini_gnb_c_lookup_tbsize(24U, 9U) == 120U, "expected DL data tbsize");
  mini_gnb_c_require(mini_gnb_c_lookup_tbsize(7U, 9U) == 0U, "expected unknown PRB/MCS pair to map to 0");
}
