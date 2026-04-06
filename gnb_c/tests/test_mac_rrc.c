#include <string.h>

#include "test_helpers.h"

#include "mini_gnb_c/common/hex.h"
#include "mini_gnb_c/config/config_loader.h"
#include "mini_gnb_c/mac/mac_ul_demux.h"
#include "mini_gnb_c/phy_dl/msg4_builder.h"
#include "mini_gnb_c/phy_ul/mock_msg3_receiver.h"
#include "mini_gnb_c/radio/mock_radio_frontend.h"
#include "mini_gnb_c/rrc/rrc_ccch_stub.h"
#include "mini_gnb_c/timing/slot_engine.h"

void test_mac_rrc_and_msg4_contention_identity(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  char contention_id_hex[MINI_GNB_C_MAX_TEXT];
  char msg4_contention_id_hex[MINI_GNB_C_MAX_TEXT];
  mini_gnb_c_config_t config;
  mini_gnb_c_slot_engine_t slot_engine;
  mini_gnb_c_mock_msg3_receiver_t msg3_receiver;
  mini_gnb_c_mock_radio_frontend_t radio;
  mini_gnb_c_slot_indication_t msg3_slot;
  mini_gnb_c_ul_grant_for_msg3_t msg3_grant;
  mini_gnb_c_radio_burst_t burst;
  mini_gnb_c_msg3_decode_indication_t msg3;
  mini_gnb_c_mac_ul_parse_result_t mac_result;
  mini_gnb_c_rrc_setup_request_info_t request_info;
  mini_gnb_c_rrc_setup_blob_t rrc_setup;
  mini_gnb_c_msg4_schedule_request_t msg4_request;
  mini_gnb_c_buffer_t msg4;

  mini_gnb_c_default_config_path(config_path, sizeof(config_path));
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) == 0,
                     "expected config to load");
  mini_gnb_c_slot_engine_init(&slot_engine, &config);
  mini_gnb_c_mock_msg3_receiver_init(&msg3_receiver, &config.sim);
  mini_gnb_c_mock_radio_frontend_init(&radio, &config.rf, &config.sim);

  mini_gnb_c_slot_engine_make_slot(&slot_engine,
                                   config.sim.prach_trigger_abs_slot + config.sim.msg3_delay_slots,
                                   &msg3_slot);
  memset(&msg3_grant, 0, sizeof(msg3_grant));
  msg3_grant.tc_rnti = 0x4601U;
  msg3_grant.abs_slot = msg3_slot.abs_slot;
  msg3_grant.msg3_prb_start = 48U;
  msg3_grant.msg3_prb_len = 16U;
  msg3_grant.msg3_mcs = 4U;
  msg3_grant.k2 = 4U;
  msg3_grant.ta_cmd = (uint8_t)config.sim.ta_est;

  mini_gnb_c_mock_radio_frontend_arm_msg3(&radio, &msg3_grant);
  mini_gnb_c_mock_radio_frontend_receive(&radio, &msg3_slot, &burst);

  mini_gnb_c_require(mini_gnb_c_mock_msg3_receiver_decode(&msg3_receiver,
                                                          &msg3_slot,
                                                          &msg3_grant,
                                                          &burst,
                                                          &msg3),
                     "expected mock Msg3 decode");
  mini_gnb_c_require(msg3.crc_ok, "expected Msg3 CRC success in default config");

  mini_gnb_c_mac_ul_demux_parse(&msg3.mac_pdu, &mac_result);
  mini_gnb_c_require(mac_result.parse_ok, "expected MAC parse success");
  mini_gnb_c_require(mac_result.has_ul_ccch, "expected UL-CCCH in Msg3");
  mini_gnb_c_require(mac_result.has_crnti_ce, "expected C-RNTI CE in Msg3");

  mini_gnb_c_parse_rrc_setup_request(&mac_result.ul_ccch_sdu, &request_info);
  mini_gnb_c_require(request_info.valid, "expected valid RRCSetupRequest");
  mini_gnb_c_require(mini_gnb_c_bytes_to_hex(request_info.contention_id48,
                                             6U,
                                             contention_id_hex,
                                             sizeof(contention_id_hex)) == 0,
                     "expected contention identity to serialize");
  mini_gnb_c_require(strcmp(contention_id_hex, config.sim.contention_id_hex) == 0,
                     "expected contention identity copied from simulated Msg3");

  mini_gnb_c_build_rrc_setup(&request_info, &config.sim, &rrc_setup);
  mini_gnb_c_require(strstr((const char*)rrc_setup.asn1_buf.bytes, "sr_period_slots=") != NULL,
                     "expected RRCSetup to carry SR period");
  mini_gnb_c_require(strstr((const char*)rrc_setup.asn1_buf.bytes, "sr_offset_slot=") != NULL,
                     "expected RRCSetup to carry SR offset");
  memset(&msg4_request, 0, sizeof(msg4_request));
  msg4_request.tc_rnti = msg3_grant.tc_rnti;
  msg4_request.msg4_abs_slot = msg3_slot.abs_slot + config.sim.msg4_delay_slots;
  memcpy(msg4_request.contention_id48, request_info.contention_id48, 6U);
  msg4_request.rrc_setup = rrc_setup;
  mini_gnb_c_build_msg4_pdu(&msg4_request, &msg4);

  mini_gnb_c_require(msg4.len >= 8U, "expected non-empty Msg4 payload");
  mini_gnb_c_require(mini_gnb_c_bytes_to_hex(&msg4.bytes[2],
                                             6U,
                                             msg4_contention_id_hex,
                                             sizeof(msg4_contention_id_hex)) == 0,
                     "expected Msg4 contention identity to serialize");
  mini_gnb_c_require(strcmp(msg4_contention_id_hex, config.sim.contention_id_hex) == 0,
                     "expected Msg4 contention identity to match Msg3");
}
