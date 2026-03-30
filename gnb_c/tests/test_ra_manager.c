#include <string.h>

#include "test_helpers.h"

#include "mini_gnb_c/config/config_loader.h"
#include "mini_gnb_c/metrics/metrics_trace.h"
#include "mini_gnb_c/ra/ra_manager.h"
#include "mini_gnb_c/timing/slot_engine.h"

void test_ra_manager_flow(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char output_dir[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  mini_gnb_c_config_t config;
  mini_gnb_c_metrics_trace_t metrics;
  mini_gnb_c_slot_engine_t slot_engine;
  mini_gnb_c_ra_manager_t ra_manager;
  mini_gnb_c_slot_indication_t prach_slot;
  mini_gnb_c_prach_indication_t prach;
  mini_gnb_c_ra_schedule_request_t rar_request;
  mini_gnb_c_slot_indication_t rar_slot;
  mini_gnb_c_msg3_decode_indication_t msg3;
  mini_gnb_c_mac_ul_parse_result_t mac_result;
  mini_gnb_c_rrc_setup_request_info_t request_info;
  mini_gnb_c_rrc_setup_blob_t rrc_setup;
  mini_gnb_c_slot_indication_t msg3_slot;
  mini_gnb_c_msg4_schedule_request_t msg4_request;
  mini_gnb_c_slot_indication_t msg4_slot;

  mini_gnb_c_default_config_path(config_path, sizeof(config_path));
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) == 0,
                     "expected config to load");
  mini_gnb_c_make_output_dir("test_ra_manager_flow_c", output_dir, sizeof(output_dir));
  mini_gnb_c_metrics_trace_init(&metrics, output_dir);
  mini_gnb_c_slot_engine_init(&slot_engine, &config);
  mini_gnb_c_ra_manager_init(&ra_manager, &config.prach, &config.sim);

  mini_gnb_c_slot_engine_make_slot(&slot_engine, config.sim.prach_trigger_abs_slot, &prach_slot);
  memset(&prach, 0, sizeof(prach));
  prach.sfn = prach_slot.sfn;
  prach.slot = prach_slot.slot;
  prach.abs_slot = prach_slot.abs_slot;
  prach.preamble_id = config.sim.preamble_id;
  prach.ta_est = config.sim.ta_est;
  prach.peak_metric = config.sim.peak_metric;
  prach.snr_est = 20.0;
  prach.valid = true;

  mini_gnb_c_require(mini_gnb_c_ra_manager_on_prach(&ra_manager,
                                                    &prach,
                                                    &prach_slot,
                                                    &metrics,
                                                    &rar_request),
                     "expected RA schedule request after PRACH");
  mini_gnb_c_require(ra_manager.has_active_context, "expected active RA context");
  mini_gnb_c_require(ra_manager.active_context.state == MINI_GNB_C_RA_TC_RNTI_ASSIGNED,
                     "expected TC-RNTI assigned state");

  mini_gnb_c_slot_engine_make_slot(&slot_engine, rar_request.rar_abs_slot, &rar_slot);
  mini_gnb_c_ra_manager_mark_rar_sent(&ra_manager, rar_request.tc_rnti, &rar_slot, &metrics);
  mini_gnb_c_require(ra_manager.active_context.state == MINI_GNB_C_RA_MSG3_WAIT,
                     "expected MSG3_WAIT after RAR");

  memset(&msg3, 0, sizeof(msg3));
  msg3.sfn = rar_slot.sfn;
  msg3.slot = rar_slot.slot;
  msg3.abs_slot = rar_request.ul_grant.abs_slot;
  msg3.rnti = rar_request.tc_rnti;
  msg3.crc_ok = true;
  msg3.snr_db = 18.2;
  msg3.evm = 2.1;

  memset(&mac_result, 0, sizeof(mac_result));
  mac_result.parse_ok = true;
  mac_result.has_ul_ccch = true;

  memset(&request_info, 0, sizeof(request_info));
  request_info.valid = true;
  request_info.establishment_cause = config.sim.establishment_cause;
  request_info.ue_identity_type = config.sim.ue_identity_type;
  request_info.contention_id48[0] = 0xA1U;
  request_info.contention_id48[1] = 0xB2U;
  request_info.contention_id48[2] = 0xC3U;
  request_info.contention_id48[3] = 0xD4U;
  request_info.contention_id48[4] = 0xE5U;
  request_info.contention_id48[5] = 0xF6U;
  mini_gnb_c_buffer_set_text(&rrc_setup.asn1_buf, "RRC");

  mini_gnb_c_slot_engine_make_slot(&slot_engine, rar_request.ul_grant.abs_slot, &msg3_slot);
  mini_gnb_c_require(mini_gnb_c_ra_manager_on_msg3_success(&ra_manager,
                                                           &msg3,
                                                           &mac_result,
                                                           &request_info,
                                                           &rrc_setup,
                                                           &msg3_slot,
                                                           &metrics,
                                                           &msg4_request),
                     "expected Msg4 scheduling request");
  mini_gnb_c_require(ra_manager.active_context.state == MINI_GNB_C_RA_MSG3_OK,
                     "expected MSG3_OK after Msg3");

  mini_gnb_c_slot_engine_make_slot(&slot_engine, msg4_request.msg4_abs_slot, &msg4_slot);
  mini_gnb_c_ra_manager_mark_msg4_sent(&ra_manager, msg4_request.tc_rnti, &msg4_slot, &metrics);
  mini_gnb_c_require(ra_manager.active_context.state == MINI_GNB_C_RA_DONE,
                     "expected DONE after Msg4 transmission");
}

void test_ra_timeout(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char output_dir[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  mini_gnb_c_config_t config;
  mini_gnb_c_metrics_trace_t metrics;
  mini_gnb_c_slot_engine_t slot_engine;
  mini_gnb_c_ra_manager_t ra_manager;
  mini_gnb_c_slot_indication_t prach_slot;
  mini_gnb_c_prach_indication_t prach;
  mini_gnb_c_ra_schedule_request_t rar_request;
  mini_gnb_c_slot_indication_t timeout_slot;

  mini_gnb_c_default_config_path(config_path, sizeof(config_path));
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) == 0,
                     "expected config to load");
  mini_gnb_c_make_output_dir("test_ra_timeout_c", output_dir, sizeof(output_dir));
  mini_gnb_c_metrics_trace_init(&metrics, output_dir);
  mini_gnb_c_slot_engine_init(&slot_engine, &config);
  mini_gnb_c_ra_manager_init(&ra_manager, &config.prach, &config.sim);

  mini_gnb_c_slot_engine_make_slot(&slot_engine, config.sim.prach_trigger_abs_slot, &prach_slot);
  memset(&prach, 0, sizeof(prach));
  prach.sfn = prach_slot.sfn;
  prach.slot = prach_slot.slot;
  prach.abs_slot = prach_slot.abs_slot;
  prach.preamble_id = config.sim.preamble_id;
  prach.ta_est = config.sim.ta_est;
  prach.peak_metric = config.sim.peak_metric;
  prach.snr_est = 20.0;
  prach.valid = true;

  mini_gnb_c_require(mini_gnb_c_ra_manager_on_prach(&ra_manager,
                                                    &prach,
                                                    &prach_slot,
                                                    &metrics,
                                                    &rar_request),
                     "expected RA schedule request after PRACH");

  mini_gnb_c_slot_engine_make_slot(&slot_engine,
                                   rar_request.rar_abs_slot + (int)config.prach.ra_resp_window + 1,
                                   &timeout_slot);
  mini_gnb_c_ra_manager_expire(&ra_manager, &timeout_slot, &metrics);
  mini_gnb_c_require(ra_manager.active_context.state == MINI_GNB_C_RA_FAIL,
                     "expected RA failure after RAR timeout");
  mini_gnb_c_require(metrics.counters.ra_timeout == 1U, "expected one RA timeout counter");
}
