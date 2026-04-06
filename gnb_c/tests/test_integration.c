#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "test_helpers.h"

#include "mini_gnb_c/common/hex.h"
#include "mini_gnb_c/common/json_utils.h"
#include "mini_gnb_c/common/simulator.h"
#include "mini_gnb_c/config/config_loader.h"
#include "mini_gnb_c/link/json_link.h"
#include "mini_gnb_c/ngap/ngap_runtime.h"
#include "mini_gnb_c/ue/mini_ue_fsm.h"
#include "mini_gnb_c/ue/mini_ue_runtime.h"

static void mini_gnb_c_write_test_cf32(const char* path, size_t sample_count) {
  FILE* file = NULL;
  size_t i = 0;

  mini_gnb_c_require(path != NULL, "expected output path for cf32 input");
  mini_gnb_c_require(mini_gnb_c_write_text_file(path, "") == 0, "expected cf32 input parent directories");
  file = fopen(path, "wb");
  mini_gnb_c_require(file != NULL, "expected cf32 input file to open");

  for (i = 0; i < sample_count; ++i) {
    const float pair[2] = {0.1f + (float)i * 0.001f, -0.05f - (float)i * 0.001f};
    mini_gnb_c_require(fwrite(pair, sizeof(float), 2U, file) == 2U, "expected cf32 pair write");
  }

  fclose(file);
}

static void mini_gnb_c_write_transport_text_file(const char* path, const char* content) {
  mini_gnb_c_require(path != NULL, "expected transport text path");
  mini_gnb_c_require(content != NULL, "expected transport text content");
  mini_gnb_c_require(mini_gnb_c_write_text_file(path, content) == 0, "expected transport text file write");
}

static void mini_gnb_c_emit_test_ue_plan(const mini_gnb_c_sim_config_t* sim, const char* exchange_dir) {
  mini_gnb_c_mini_ue_fsm_t fsm;
  mini_gnb_c_ue_event_t event;

  mini_gnb_c_require(sim != NULL, "expected sim config for UE plan");
  mini_gnb_c_require(exchange_dir != NULL, "expected exchange directory for UE plan");

  mini_gnb_c_mini_ue_fsm_init(&fsm, sim);
  while (mini_gnb_c_mini_ue_fsm_next_event(&fsm, &event) > 0) {
    char payload_json[2048];
    char emitted_path[MINI_GNB_C_MAX_PATH];

    mini_gnb_c_require(mini_gnb_c_ue_event_build_payload_json(&event, payload_json, sizeof(payload_json)) == 0,
                       "expected UE event payload JSON");
    mini_gnb_c_require(mini_gnb_c_json_link_emit_event(exchange_dir,
                                                       "ue_to_gnb",
                                                       "ue",
                                                       mini_gnb_c_ue_event_type_to_string(event.type),
                                                       event.sequence,
                                                       event.abs_slot,
                                                       payload_json,
                                                       emitted_path,
                                                       sizeof(emitted_path)) == 0,
                       "expected local exchange event emission");
  }
}

static void mini_gnb_c_disable_local_exchange(mini_gnb_c_config_t* config) {
  mini_gnb_c_require(config != NULL, "expected config to disable local exchange");
  config->sim.local_exchange_dir[0] = '\0';
}

typedef struct {
  uint8_t responses[8][MINI_GNB_C_CORE_BRIDGE_MAX_MESSAGE];
  size_t response_lengths[8];
  size_t response_count;
  size_t response_index;
  uint8_t sent_messages[8][MINI_GNB_C_CORE_BRIDGE_MAX_MESSAGE];
  size_t sent_lengths[8];
  size_t sent_count;
} mini_gnb_c_integration_fake_ngap_transport_t;

static int mini_gnb_c_integration_fake_transport_connect(mini_gnb_c_ngap_transport_t* transport,
                                                         const char* amf_ip,
                                                         uint32_t amf_port,
                                                         uint32_t timeout_ms) {
  (void)amf_ip;
  (void)amf_port;
  (void)timeout_ms;
  mini_gnb_c_require(transport != NULL, "expected transport in integration fake connect");
  transport->socket_fd = 1;
  return 0;
}

static int mini_gnb_c_integration_fake_transport_send(mini_gnb_c_ngap_transport_t* transport,
                                                      const uint8_t* bytes,
                                                      size_t length) {
  mini_gnb_c_integration_fake_ngap_transport_t* fake = NULL;

  mini_gnb_c_require(transport != NULL && bytes != NULL && length > 0u,
                     "expected fake integration send arguments");
  fake = (mini_gnb_c_integration_fake_ngap_transport_t*)transport->user_data;
  mini_gnb_c_require(fake != NULL, "expected fake integration transport state");
  mini_gnb_c_require(fake->sent_count < 8u, "unexpected extra integration send");
  mini_gnb_c_require(length <= sizeof(fake->sent_messages[0]), "expected integration send capacity");
  memcpy(fake->sent_messages[fake->sent_count], bytes, length);
  fake->sent_lengths[fake->sent_count] = length;
  ++fake->sent_count;
  return 0;
}

static int mini_gnb_c_integration_fake_transport_recv(mini_gnb_c_ngap_transport_t* transport,
                                                      uint8_t* response,
                                                      size_t response_capacity,
                                                      size_t* response_length) {
  mini_gnb_c_integration_fake_ngap_transport_t* fake = NULL;
  size_t length = 0u;

  mini_gnb_c_require(transport != NULL && response != NULL && response_length != NULL,
                     "expected fake integration recv arguments");
  fake = (mini_gnb_c_integration_fake_ngap_transport_t*)transport->user_data;
  mini_gnb_c_require(fake != NULL, "expected fake integration transport state");
  mini_gnb_c_require(fake->response_index < fake->response_count, "unexpected extra integration recv");
  length = fake->response_lengths[fake->response_index];
  mini_gnb_c_require(length <= response_capacity, "expected integration recv capacity");
  memcpy(response, fake->responses[fake->response_index], length);
  *response_length = length;
  ++fake->response_index;
  return 0;
}

static void mini_gnb_c_integration_fake_transport_close(mini_gnb_c_ngap_transport_t* transport) {
  if (transport != NULL) {
    transport->socket_fd = -1;
  }
}

static const mini_gnb_c_ngap_transport_ops_t k_mini_gnb_c_integration_fake_transport_ops = {
    mini_gnb_c_integration_fake_transport_connect,
    mini_gnb_c_integration_fake_transport_send,
    mini_gnb_c_integration_fake_transport_recv,
    mini_gnb_c_integration_fake_transport_close,
};

static void mini_gnb_c_prepare_integration_fake_amf_dialog(mini_gnb_c_integration_fake_ngap_transport_t* fake) {
  static const uint8_t k_ngsetup_response[] = {0x20u, 0x15u, 0x00u, 0x00u};
  static const uint8_t k_auth_request_nas[] = {0x7eu, 0x00u, 0x56u, 0x01u, 0x02u, 0x03u};
  uint8_t message[MINI_GNB_C_CORE_BRIDGE_MAX_MESSAGE];
  size_t message_length = 0u;

  mini_gnb_c_require(fake != NULL, "expected fake integration transport");
  memset(fake, 0, sizeof(*fake));
  memcpy(fake->responses[0], k_ngsetup_response, sizeof(k_ngsetup_response));
  fake->response_lengths[0] = sizeof(k_ngsetup_response);
  mini_gnb_c_require(mini_gnb_c_ngap_build_downlink_nas_transport(0x1234u,
                                                                  1u,
                                                                  k_auth_request_nas,
                                                                  sizeof(k_auth_request_nas),
                                                                  message,
                                                                  sizeof(message),
                                                                  &message_length) == 0,
                     "expected canned integration DownlinkNASTransport");
  memcpy(fake->responses[1], message, message_length);
  fake->response_lengths[1] = message_length;
  fake->response_count = 2u;
}

static void mini_gnb_c_prepare_integration_fake_amf_dialog_with_followup(
    mini_gnb_c_integration_fake_ngap_transport_t* fake) {
  static const uint8_t k_security_mode_command_nas[] = {0x7eu, 0x00u, 0x5du, 0x11u, 0x22u, 0x33u};
  uint8_t message[MINI_GNB_C_CORE_BRIDGE_MAX_MESSAGE];
  size_t message_length = 0u;

  mini_gnb_c_prepare_integration_fake_amf_dialog(fake);
  mini_gnb_c_require(mini_gnb_c_ngap_build_downlink_nas_transport(0x1234u,
                                                                  1u,
                                                                  k_security_mode_command_nas,
                                                                  sizeof(k_security_mode_command_nas),
                                                                  message,
                                                                  sizeof(message),
                                                                  &message_length) == 0,
                     "expected canned follow-up integration DownlinkNASTransport");
  memcpy(fake->responses[2], message, message_length);
  fake->response_lengths[2] = message_length;
  fake->response_count = 3u;
}

static void mini_gnb_c_prepare_integration_fake_amf_dialog_with_session_setup(
    mini_gnb_c_integration_fake_ngap_transport_t* fake) {
  static const uint8_t k_pdu_session_resource_setup_request[] = {
      0x00u, 0x1du, 0x00u, 0x29u,
      0x00u, 0x00u, 0x01u,
      0x00u, 0x4au, 0x00u, 0x22u,
      0x7eu, 0x00u, 0x68u, 0x01u,
      0x29u, 0x05u, 0x01u, 10u, 45u, 0u, 7u,
      0x00u, 0x00u, 0x02u,
      0x00u, 0x8bu, 0x00u, 0x0au, 0x00u, 0xf0u, 127u, 0u, 0u, 7u, 0x00u, 0x00u, 0xefu, 0x26u,
      0x00u, 0x88u, 0x00u, 0x02u, 0x00u, 0x01u,
  };
  uint8_t message[MINI_GNB_C_CORE_BRIDGE_MAX_MESSAGE];
  size_t message_length = 0u;

  mini_gnb_c_prepare_integration_fake_amf_dialog_with_followup(fake);
  mini_gnb_c_require(mini_gnb_c_ngap_build_initial_context_setup_response(0x1234u,
                                                                          1u,
                                                                          message,
                                                                          sizeof(message),
                                                                          &message_length) == 0,
                     "expected canned InitialContextSetupRequest seed");
  message[0] = 0x00u;
  memcpy(fake->responses[3], message, message_length);
  fake->response_lengths[3] = message_length;
  memcpy(fake->responses[4],
         k_pdu_session_resource_setup_request,
         sizeof(k_pdu_session_resource_setup_request));
  fake->response_lengths[4] = sizeof(k_pdu_session_resource_setup_request);
  fake->response_count = 5u;
}

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
  mini_gnb_c_disable_local_exchange(&config);
  mini_gnb_c_make_output_dir("test_integration_c", output_dir, sizeof(output_dir));
  mini_gnb_c_simulator_init(&simulator, &config, output_dir);
  mini_gnb_c_require(mini_gnb_c_simulator_run(&simulator, &summary) == 0, "expected simulator run");

  mini_gnb_c_require(summary.counters.prach_detect_ok >= 1U, "expected PRACH detection");
  mini_gnb_c_require(summary.counters.rar_sent >= 1U, "expected RAR transmission");
  mini_gnb_c_require(summary.counters.msg3_crc_ok >= 1U, "expected Msg3 CRC success");
  mini_gnb_c_require(summary.counters.rrcsetup_sent >= 1U, "expected Msg4/RRCSetup transmission");
  mini_gnb_c_require(summary.counters.pucch_sr_detect_ok >= 1U, "expected post-Msg4 PUCCH SR detection");
  mini_gnb_c_require(summary.counters.ul_bsr_rx_ok >= 1U, "expected scheduled UL BSR reception");
  mini_gnb_c_require(summary.counters.dl_data_sent >= 1U, "expected post-Msg4 PUCCH config transmission");
  mini_gnb_c_require(summary.counters.ul_data_rx_ok >= 1U, "expected large scheduled UL data reception");
  mini_gnb_c_require(summary.ue_count > 0U, "expected at least one promoted UE context");
  mini_gnb_c_require(summary.ue_contexts[0].rrc_setup_sent, "expected UE context marked after Msg4");
  mini_gnb_c_require(summary.ue_contexts[0].dl_data_sent, "expected UE context marked after PUCCH config");
  mini_gnb_c_require(summary.ue_contexts[0].pucch_sr_detected, "expected UE context marked after PUCCH SR");
  mini_gnb_c_require(summary.ue_contexts[0].ul_bsr_received, "expected UE context marked after UL BSR");
  mini_gnb_c_require(summary.ue_contexts[0].ul_data_received, "expected UE context marked after UL data");
  mini_gnb_c_require(summary.ue_contexts[0].core_session.c_rnti == summary.ue_contexts[0].c_rnti,
                     "expected UE core session seed to follow promoted C-RNTI");
  mini_gnb_c_require(!summary.ue_contexts[0].core_session.ran_ue_ngap_id_valid,
                     "expected no NGAP state before Stage C bridge");
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
  mini_gnb_c_require(strstr(summary_json, "\"core_session\":{\"c_rnti\":17921") != NULL,
                     "expected summary JSON core session block");

  (void)snprintf(iq_cf32_name,
                 sizeof(iq_cf32_name),
                 "slot_7_DL_OBJ_MSG4_rnti_%u.cf32",
                 summary.ue_contexts[0].c_rnti);
  (void)snprintf(iq_json_name,
                 sizeof(iq_json_name),
                 "slot_7_DL_OBJ_MSG4_rnti_%u.json",
                 summary.ue_contexts[0].c_rnti);
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

void test_integration_slot_input_prach(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char output_dir[MINI_GNB_C_MAX_PATH];
  char input_dir[MINI_GNB_C_MAX_PATH];
  char prach_path[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  mini_gnb_c_config_t config;
  mini_gnb_c_simulator_t simulator;
  mini_gnb_c_run_summary_t summary;

  mini_gnb_c_default_config_path(config_path, sizeof(config_path));
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) == 0,
                     "expected config to load");
  mini_gnb_c_disable_local_exchange(&config);

  config.sim.total_slots = 24;
  config.sim.msg3_present = false;
  config.sim.prach_retry_delay_slots = -1;
  config.broadcast.prach_period_slots = 1;
  config.broadcast.prach_offset_slot = 0;

  mini_gnb_c_make_output_dir("test_integration_slot_input_c", output_dir, sizeof(output_dir));
  mini_gnb_c_require(mini_gnb_c_join_path(output_dir, "input", input_dir, sizeof(input_dir)) == 0,
                     "expected input directory path");
  mini_gnb_c_reset_test_dir(input_dir);
  mini_gnb_c_require(snprintf(config.sim.ul_input_dir,
                              sizeof(config.sim.ul_input_dir),
                              "%s",
                              input_dir) < (int)sizeof(config.sim.ul_input_dir),
                     "expected input directory to fit in config");
  mini_gnb_c_require(snprintf(prach_path,
                              sizeof(prach_path),
                              "%s/slot_20_UL_OBJ_PRACH.cf32",
                              input_dir) < (int)sizeof(prach_path),
                     "expected PRACH input path");
  mini_gnb_c_write_test_cf32(prach_path, 64U);

  mini_gnb_c_simulator_init(&simulator, &config, output_dir);
  mini_gnb_c_require(mini_gnb_c_simulator_run(&simulator, &summary) == 0, "expected simulator run");

  mini_gnb_c_require(summary.counters.prach_detect_ok == 1U, "expected exactly one file-driven PRACH detection");
  mini_gnb_c_require(summary.counters.rar_sent == 1U, "expected one RAR after file-driven PRACH");
  mini_gnb_c_require(summary.counters.msg3_crc_ok == 0U, "expected no Msg3 decode without slot input file");
  mini_gnb_c_require(summary.has_ra_context, "expected RA context after file-driven PRACH");
  mini_gnb_c_require(summary.ra_context.detect_abs_slot == 20, "expected file-driven PRACH to start at abs_slot 20");
  mini_gnb_c_require(summary.ra_context.rar_abs_slot == 21, "expected RAR immediately after slot-input PRACH");
  mini_gnb_c_require(summary.ra_context.state == MINI_GNB_C_RA_MSG3_WAIT,
                     "expected RAR sent and Msg3 wait state at loop end");
}

void test_integration_local_exchange_ue_plan(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char output_dir[MINI_GNB_C_MAX_PATH];
  char exchange_dir[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  mini_gnb_c_config_t ue_config;
  mini_gnb_c_config_t gnb_config;
  mini_gnb_c_simulator_t simulator;
  mini_gnb_c_run_summary_t summary;
  char* summary_json = NULL;

  mini_gnb_c_default_config_path(config_path, sizeof(config_path));
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &ue_config, error_message, sizeof(error_message)) == 0,
                     "expected UE config to load");
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &gnb_config, error_message, sizeof(error_message)) == 0,
                     "expected gNB config to load");

  mini_gnb_c_make_output_dir("test_local_exchange_loop", output_dir, sizeof(output_dir));
  mini_gnb_c_require(mini_gnb_c_join_path(output_dir, "local_exchange", exchange_dir, sizeof(exchange_dir)) == 0,
                     "expected local exchange directory path");

  (void)snprintf(ue_config.sim.local_exchange_dir, sizeof(ue_config.sim.local_exchange_dir), "%s", exchange_dir);
  mini_gnb_c_emit_test_ue_plan(&ue_config.sim, exchange_dir);

  (void)snprintf(gnb_config.sim.local_exchange_dir, sizeof(gnb_config.sim.local_exchange_dir), "%s", exchange_dir);
  gnb_config.sim.ul_input_dir[0] = '\0';
  gnb_config.sim.msg3_present = false;
  gnb_config.sim.ul_data_present = false;

  mini_gnb_c_simulator_init(&simulator, &gnb_config, output_dir);
  mini_gnb_c_require(mini_gnb_c_simulator_run(&simulator, &summary) == 0,
                     "expected simulator run from local exchange events");

  mini_gnb_c_require(summary.counters.prach_detect_ok >= 1U, "expected PRACH from local exchange");
  mini_gnb_c_require(summary.counters.msg3_crc_ok >= 1U, "expected Msg3 from local exchange");
  mini_gnb_c_require(summary.counters.pucch_sr_detect_ok >= 1U, "expected PUCCH SR from local exchange");
  mini_gnb_c_require(summary.counters.ul_bsr_rx_ok >= 1U, "expected BSR from local exchange");
  mini_gnb_c_require(summary.counters.ul_data_rx_ok >= 1U, "expected UL data from local exchange");
  mini_gnb_c_require(summary.ue_count == 1U, "expected one promoted UE from local exchange");
  mini_gnb_c_require(summary.ue_contexts[0].tc_rnti == 0x4601U, "expected deterministic UE RNTI");
  mini_gnb_c_require(summary.ue_contexts[0].ul_bsr_buffer_size_bytes == 384, "expected BSR size from UE plan");
  mini_gnb_c_require(summary.ue_contexts[0].core_session.c_rnti == 0x4601U,
                     "expected local exchange UE context to seed core session");

  summary_json = mini_gnb_c_read_text_file(summary.summary_path);
  mini_gnb_c_require(summary_json != NULL, "expected local exchange summary");
  mini_gnb_c_require(strstr(summary_json, "\"ul_data_rx_ok\":1") != NULL,
                     "expected summary counters for local exchange run");
  mini_gnb_c_require(strstr(summary_json, "\"core_session\":{\"c_rnti\":17921") != NULL,
                     "expected local exchange summary core session block");
  free(summary_json);
}

void test_integration_shared_slot_ue_runtime(void) {
  char gnb_config_path[MINI_GNB_C_MAX_PATH];
  char ue_config_path[MINI_GNB_C_MAX_PATH];
  char output_dir[MINI_GNB_C_MAX_PATH];
  char shared_slot_path[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  mini_gnb_c_config_t ue_config;
  mini_gnb_c_config_t gnb_config;
  mini_gnb_c_simulator_t simulator;
  mini_gnb_c_run_summary_t summary;
  char* summary_json = NULL;
  pid_t child_pid = -1;
  int status = 0;

  mini_gnb_c_require(snprintf(gnb_config_path,
                              sizeof(gnb_config_path),
                              "%s/config/example_shared_slot_gnb.yml",
                              MINI_GNB_C_SOURCE_DIR) < (int)sizeof(gnb_config_path),
                     "expected shared-slot gNB config path");
  mini_gnb_c_require(snprintf(ue_config_path,
                              sizeof(ue_config_path),
                              "%s/config/example_shared_slot_ue.yml",
                              MINI_GNB_C_SOURCE_DIR) < (int)sizeof(ue_config_path),
                     "expected shared-slot UE config path");
  mini_gnb_c_require(mini_gnb_c_load_config(ue_config_path, &ue_config, error_message, sizeof(error_message)) == 0,
                     "expected UE config to load");
  mini_gnb_c_require(mini_gnb_c_load_config(gnb_config_path, &gnb_config, error_message, sizeof(error_message)) == 0,
                     "expected gNB config to load");

  mini_gnb_c_make_output_dir("test_shared_slot_loop", output_dir, sizeof(output_dir));
  mini_gnb_c_require(mini_gnb_c_join_path(output_dir, "shared_slot_link.bin", shared_slot_path, sizeof(shared_slot_path)) ==
                         0,
                     "expected shared slot link path");

  (void)snprintf(ue_config.sim.shared_slot_path, sizeof(ue_config.sim.shared_slot_path), "%s", shared_slot_path);
  ue_config.sim.shared_slot_timeout_ms = 250u;
  mini_gnb_c_disable_local_exchange(&ue_config);

  (void)snprintf(gnb_config.sim.shared_slot_path, sizeof(gnb_config.sim.shared_slot_path), "%s", shared_slot_path);
  gnb_config.sim.shared_slot_timeout_ms = 250u;
  gnb_config.sim.ul_input_dir[0] = '\0';
  mini_gnb_c_disable_local_exchange(&gnb_config);

  child_pid = fork();
  mini_gnb_c_require(child_pid >= 0, "expected fork for shared-slot UE runtime");
  if (child_pid == 0) {
    const int child_result = mini_gnb_c_run_shared_ue_runtime(&ue_config);
    _exit(child_result == 0 ? 0 : 1);
  }

  mini_gnb_c_simulator_init(&simulator, &gnb_config, output_dir);
  mini_gnb_c_require(mini_gnb_c_simulator_run(&simulator, &summary) == 0,
                     "expected simulator run from shared-slot runtime");
  mini_gnb_c_require(waitpid(child_pid, &status, 0) == child_pid, "expected shared-slot UE child completion");
  mini_gnb_c_require(WIFEXITED(status) && WEXITSTATUS(status) == 0, "expected clean shared-slot UE exit");

  mini_gnb_c_require(summary.counters.prach_detect_ok >= 1U, "expected PRACH from shared-slot runtime");
  mini_gnb_c_require(summary.counters.msg3_crc_ok >= 1U, "expected Msg3 from shared-slot runtime");
  mini_gnb_c_require(summary.counters.pucch_sr_detect_ok >= 1U, "expected PUCCH SR from shared-slot runtime");
  mini_gnb_c_require(summary.counters.ul_bsr_rx_ok >= 1U, "expected BSR from shared-slot runtime");
  mini_gnb_c_require(summary.counters.ul_data_rx_ok >= 1U, "expected UL data from shared-slot runtime");
  mini_gnb_c_require(summary.ue_count == 1U, "expected one promoted UE from shared-slot runtime");
  mini_gnb_c_require(summary.ue_contexts[0].tc_rnti == 0x4601U, "expected deterministic UE RNTI");

  summary_json = mini_gnb_c_read_text_file(summary.summary_path);
  mini_gnb_c_require(summary_json != NULL, "expected shared-slot summary");
  mini_gnb_c_require(strstr(summary_json, "\"ul_data_rx_ok\":1") != NULL,
                     "expected shared-slot summary counters");
  free(summary_json);
}

void test_integration_core_bridge_prepares_initial_message(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char output_dir[MINI_GNB_C_MAX_PATH];
  char exchange_dir[MINI_GNB_C_MAX_PATH];
  char dl_nas_event_path[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  mini_gnb_c_config_t config;
  mini_gnb_c_simulator_t simulator;
  mini_gnb_c_run_summary_t summary;
  mini_gnb_c_integration_fake_ngap_transport_t fake_transport;
  char* summary_json = NULL;
  char* dl_nas_event_json = NULL;

  mini_gnb_c_default_config_path(config_path, sizeof(config_path));
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) == 0,
                     "expected config to load");
  config.core.enabled = true;
  config.core.timeout_ms = 2500u;
  config.core.ran_ue_ngap_id_base = 1u;
  config.core.default_pdu_session_id = 1u;

  mini_gnb_c_make_output_dir("test_integration_core_bridge_c", output_dir, sizeof(output_dir));
  mini_gnb_c_require(mini_gnb_c_join_path(output_dir, "local_exchange", exchange_dir, sizeof(exchange_dir)) == 0,
                     "expected local exchange dir for core bridge test");
  (void)snprintf(config.sim.local_exchange_dir, sizeof(config.sim.local_exchange_dir), "%s", exchange_dir);
  mini_gnb_c_simulator_init(&simulator, &config, output_dir);
  mini_gnb_c_prepare_integration_fake_amf_dialog(&fake_transport);
  mini_gnb_c_ngap_transport_set_ops(&simulator.core_bridge.transport,
                                    &k_mini_gnb_c_integration_fake_transport_ops,
                                    &fake_transport);
  mini_gnb_c_require(mini_gnb_c_simulator_run(&simulator, &summary) == 0, "expected simulator run");

  mini_gnb_c_require(summary.ue_count == 1U, "expected one promoted UE");
  mini_gnb_c_require(summary.ue_contexts[0].core_session.ran_ue_ngap_id_valid &&
                         summary.ue_contexts[0].core_session.ran_ue_ngap_id == 1u,
                     "expected bridge-seeded RAN UE NGAP ID");
  mini_gnb_c_require(summary.ue_contexts[0].core_session.pdu_session_id_valid &&
                         summary.ue_contexts[0].core_session.pdu_session_id == 1u,
                     "expected bridge-seeded requested PDU session ID");
  mini_gnb_c_require(summary.ue_contexts[0].core_session.amf_ue_ngap_id_valid &&
                         summary.ue_contexts[0].core_session.amf_ue_ngap_id == 0x1234u,
                     "expected parsed AMF UE NGAP ID");
  mini_gnb_c_require(summary.ue_contexts[0].core_session.uplink_nas_count == 1u,
                     "expected sent InitialUEMessage uplink NAS count");
  mini_gnb_c_require(summary.ue_contexts[0].core_session.downlink_nas_count == 1u,
                     "expected received first downlink NAS count");
  mini_gnb_c_require(simulator.core_bridge.ng_setup_complete, "expected NGSetup to complete");
  mini_gnb_c_require(simulator.core_bridge.last_initial_ue_message_length > 0u,
                     "expected stored InitialUEMessage in bridge");
  mini_gnb_c_require(simulator.core_bridge.last_initial_ue_message[0] == 0x00u &&
                         simulator.core_bridge.last_initial_ue_message[1] == 0x0fu,
                     "expected InitialUEMessage header in stored bridge message");
  mini_gnb_c_require(simulator.core_bridge.last_downlink_nas_length == 6u,
                     "expected stored downlink NAS in bridge");
  mini_gnb_c_require(fake_transport.sent_count == 2u, "expected NGSetup and InitialUEMessage sends");
  mini_gnb_c_require(fake_transport.sent_messages[0][0] == 0x00u &&
                         fake_transport.sent_messages[0][1] == 0x15u,
                     "expected NGSetupRequest send before InitialUEMessage");

  summary_json = mini_gnb_c_read_text_file(summary.summary_path);
  mini_gnb_c_require(summary_json != NULL, "expected core bridge summary");
  mini_gnb_c_require(strstr(summary_json, "\"ran_ue_ngap_id\":1") != NULL,
                     "expected summary JSON RAN UE NGAP ID");
  mini_gnb_c_require(strstr(summary_json, "\"amf_ue_ngap_id\":4660") != NULL,
                     "expected summary JSON AMF UE NGAP ID");
  mini_gnb_c_require(strstr(summary_json, "\"uplink_nas_count\":1") != NULL,
                     "expected summary JSON uplink NAS count");
  mini_gnb_c_require(strstr(summary_json, "\"downlink_nas_count\":1") != NULL,
                     "expected summary JSON downlink NAS count");

  mini_gnb_c_require(mini_gnb_c_json_link_build_event_path(exchange_dir,
                                                           "gnb_to_ue",
                                                           "gnb",
                                                           1u,
                                                           "DL_NAS",
                                                           dl_nas_event_path,
                                                           sizeof(dl_nas_event_path)) == 0,
                     "expected downlink NAS event path");
  dl_nas_event_json = mini_gnb_c_read_text_file(dl_nas_event_path);
  mini_gnb_c_require(dl_nas_event_json != NULL, "expected downlink NAS event JSON");
  mini_gnb_c_require(strstr(dl_nas_event_json, "\"type\": \"DL_NAS\"") != NULL,
                     "expected downlink NAS event type");
  mini_gnb_c_require(strstr(dl_nas_event_json, "\"amf_ue_ngap_id\":4660") != NULL,
                     "expected downlink NAS event payload AMF UE NGAP ID");
  mini_gnb_c_require(strstr(dl_nas_event_json, "\"nas_hex\":\"7E0056010203\"") != NULL,
                     "expected downlink NAS event payload NAS hex");
  free(dl_nas_event_json);
  free(summary_json);
}

void test_integration_core_bridge_relays_followup_ul_nas(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char output_dir[MINI_GNB_C_MAX_PATH];
  char exchange_dir[MINI_GNB_C_MAX_PATH];
  char second_dl_nas_event_path[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  mini_gnb_c_config_t config;
  mini_gnb_c_simulator_t simulator;
  mini_gnb_c_run_summary_t summary;
  mini_gnb_c_integration_fake_ngap_transport_t fake_transport;
  char* summary_json = NULL;
  char* second_dl_nas_event_json = NULL;

  mini_gnb_c_default_config_path(config_path, sizeof(config_path));
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) == 0,
                     "expected config to load");
  config.core.enabled = true;
  config.core.timeout_ms = 2500u;
  config.core.ran_ue_ngap_id_base = 1u;
  config.core.default_pdu_session_id = 1u;

  mini_gnb_c_make_output_dir("test_integration_core_bridge_followup_c", output_dir, sizeof(output_dir));
  mini_gnb_c_require(mini_gnb_c_join_path(output_dir, "local_exchange", exchange_dir, sizeof(exchange_dir)) == 0,
                     "expected local exchange dir for follow-up core bridge test");
  (void)snprintf(config.sim.local_exchange_dir, sizeof(config.sim.local_exchange_dir), "%s", exchange_dir);
  mini_gnb_c_require(mini_gnb_c_json_link_emit_event(exchange_dir,
                                                     "ue_to_gnb_nas",
                                                     "ue",
                                                     "UL_NAS",
                                                     1u,
                                                     7,
                                                     "{\"c_rnti\":17921,\"nas_hex\":\"7E005C000D0164F099F0FF00002143658789\"}",
                                                     NULL,
                                                     0u) == 0,
                     "expected seeded follow-up UL_NAS event");
  mini_gnb_c_prepare_integration_fake_amf_dialog_with_followup(&fake_transport);

  mini_gnb_c_simulator_init(&simulator, &config, output_dir);
  mini_gnb_c_ngap_transport_set_ops(&simulator.core_bridge.transport,
                                    &k_mini_gnb_c_integration_fake_transport_ops,
                                    &fake_transport);
  mini_gnb_c_require(mini_gnb_c_simulator_run(&simulator, &summary) == 0, "expected simulator run");

  mini_gnb_c_require(summary.ue_count == 1u, "expected one promoted UE");
  mini_gnb_c_require(summary.ue_contexts[0].core_session.uplink_nas_count == 2u,
                     "expected two uplink NAS messages after follow-up relay");
  mini_gnb_c_require(summary.ue_contexts[0].core_session.downlink_nas_count == 2u,
                     "expected two downlink NAS messages after follow-up relay");
  mini_gnb_c_require(simulator.core_bridge.next_ue_to_gnb_nas_sequence == 2u,
                     "expected consumed follow-up UL_NAS event");
  mini_gnb_c_require(simulator.core_bridge.next_gnb_to_ue_sequence == 3u,
                     "expected two emitted DL_NAS events");
  mini_gnb_c_require(fake_transport.sent_count == 3u, "expected NGSetup plus two NAS uplinks");
  mini_gnb_c_require(fake_transport.sent_messages[2][0] == 0x00u &&
                         fake_transport.sent_messages[2][1] == 0x2eu,
                     "expected follow-up UplinkNASTransport message");

  summary_json = mini_gnb_c_read_text_file(summary.summary_path);
  mini_gnb_c_require(summary_json != NULL, "expected follow-up core bridge summary");
  mini_gnb_c_require(strstr(summary_json, "\"uplink_nas_count\":2") != NULL,
                     "expected summary JSON follow-up uplink NAS count");
  mini_gnb_c_require(strstr(summary_json, "\"downlink_nas_count\":2") != NULL,
                     "expected summary JSON follow-up downlink NAS count");

  mini_gnb_c_require(mini_gnb_c_json_link_build_event_path(exchange_dir,
                                                           "gnb_to_ue",
                                                           "gnb",
                                                           2u,
                                                           "DL_NAS",
                                                           second_dl_nas_event_path,
                                                           sizeof(second_dl_nas_event_path)) == 0,
                     "expected second downlink NAS event path");
  second_dl_nas_event_json = mini_gnb_c_read_text_file(second_dl_nas_event_path);
  mini_gnb_c_require(second_dl_nas_event_json != NULL, "expected second downlink NAS event JSON");
  mini_gnb_c_require(strstr(second_dl_nas_event_json, "\"nas_hex\":\"7E005D112233\"") != NULL,
                     "expected second downlink NAS payload");

  free(second_dl_nas_event_json);
  free(summary_json);
}

void test_integration_core_bridge_extracts_session_setup_state(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char output_dir[MINI_GNB_C_MAX_PATH];
  char exchange_dir[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  mini_gnb_c_config_t config;
  mini_gnb_c_simulator_t simulator;
  mini_gnb_c_run_summary_t summary;
  mini_gnb_c_integration_fake_ngap_transport_t fake_transport;
  char* summary_json = NULL;

  mini_gnb_c_default_config_path(config_path, sizeof(config_path));
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) == 0,
                     "expected config to load");
  config.core.enabled = true;
  config.core.timeout_ms = 2500u;
  config.core.ran_ue_ngap_id_base = 1u;
  config.core.default_pdu_session_id = 1u;

  mini_gnb_c_make_output_dir("test_integration_core_bridge_session_setup_c", output_dir, sizeof(output_dir));
  mini_gnb_c_require(mini_gnb_c_join_path(output_dir, "local_exchange", exchange_dir, sizeof(exchange_dir)) == 0,
                     "expected local exchange dir for session setup core bridge test");
  (void)snprintf(config.sim.local_exchange_dir, sizeof(config.sim.local_exchange_dir), "%s", exchange_dir);
  mini_gnb_c_require(mini_gnb_c_json_link_emit_event(exchange_dir,
                                                     "ue_to_gnb_nas",
                                                     "ue",
                                                     "UL_NAS",
                                                     1u,
                                                     7,
                                                     "{\"c_rnti\":17921,\"nas_hex\":\"7E005C000D0164F099F0FF00002143658789\"}",
                                                     NULL,
                                                     0u) == 0,
                     "expected first seeded follow-up UL_NAS event");
  mini_gnb_c_require(mini_gnb_c_json_link_emit_event(exchange_dir,
                                                     "ue_to_gnb_nas",
                                                     "ue",
                                                     "UL_NAS",
                                                     2u,
                                                     8,
                                                     "{\"c_rnti\":17921,\"nas_hex\":\"7E005E7700098526610956163978F871002E\"}",
                                                     NULL,
                                                     0u) == 0,
                     "expected second seeded follow-up UL_NAS event");
  mini_gnb_c_require(mini_gnb_c_json_link_emit_event(exchange_dir,
                                                     "ue_to_gnb_nas",
                                                     "ue",
                                                     "UL_NAS",
                                                     3u,
                                                     9,
                                                     "{\"c_rnti\":17921,\"nas_hex\":\"7E004301\"}",
                                                     NULL,
                                                     0u) == 0,
                     "expected third seeded follow-up UL_NAS event");

  mini_gnb_c_prepare_integration_fake_amf_dialog_with_session_setup(&fake_transport);
  mini_gnb_c_simulator_init(&simulator, &config, output_dir);
  mini_gnb_c_ngap_transport_set_ops(&simulator.core_bridge.transport,
                                    &k_mini_gnb_c_integration_fake_transport_ops,
                                    &fake_transport);
  mini_gnb_c_require(mini_gnb_c_simulator_run(&simulator, &summary) == 0, "expected simulator run");

  mini_gnb_c_require(summary.ue_count == 1u, "expected one promoted UE");
  mini_gnb_c_require(summary.ue_contexts[0].core_session.uplink_nas_count == 4u,
                     "expected session-setup uplink NAS sequence in summary");
  mini_gnb_c_require(summary.ue_contexts[0].core_session.downlink_nas_count == 2u,
                     "expected only top-level downlink NAS PDUs in summary");
  mini_gnb_c_require(strcmp(summary.ue_contexts[0].core_session.upf_ip, "127.0.0.7") == 0,
                     "expected summary UPF IP");
  mini_gnb_c_require(summary.ue_contexts[0].core_session.upf_teid == 0x0000ef26u,
                     "expected summary UPF TEID");
  mini_gnb_c_require(summary.ue_contexts[0].core_session.qfi_valid &&
                         summary.ue_contexts[0].core_session.qfi == 1u,
                     "expected summary QFI");
  mini_gnb_c_require(summary.ue_contexts[0].core_session.ue_ipv4_valid &&
                         summary.ue_contexts[0].core_session.ue_ipv4[0] == 10u &&
                         summary.ue_contexts[0].core_session.ue_ipv4[1] == 45u &&
                         summary.ue_contexts[0].core_session.ue_ipv4[2] == 0u &&
                         summary.ue_contexts[0].core_session.ue_ipv4[3] == 7u,
                     "expected summary UE IPv4 bytes");
  mini_gnb_c_require(fake_transport.sent_count == 7u, "expected session-setup NGAP acknowledgements");
  mini_gnb_c_require(fake_transport.sent_messages[4][0] == 0x20u &&
                         fake_transport.sent_messages[4][1] == 0x0eu,
                     "expected InitialContextSetupResponse during simulator run");
  mini_gnb_c_require(fake_transport.sent_messages[6][0] == 0x20u &&
                         fake_transport.sent_messages[6][1] == 0x1du,
                     "expected PDUSessionResourceSetupResponse during simulator run");

  summary_json = mini_gnb_c_read_text_file(summary.summary_path);
  mini_gnb_c_require(summary_json != NULL, "expected session setup core bridge summary");
  mini_gnb_c_require(strstr(summary_json, "\"upf_ip\":\"127.0.0.7\"") != NULL,
                     "expected summary JSON UPF IP");
  mini_gnb_c_require(strstr(summary_json, "\"upf_teid\":61222") != NULL,
                     "expected summary JSON UPF TEID");
  mini_gnb_c_require(strstr(summary_json, "\"qfi\":1") != NULL,
                     "expected summary JSON QFI");
  mini_gnb_c_require(strstr(summary_json, "\"ue_ipv4\":\"10.45.0.7\"") != NULL,
                     "expected summary JSON UE IPv4");
  free(summary_json);
}

void test_integration_slot_text_transport(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char output_dir[MINI_GNB_C_MAX_PATH];
  char input_dir[MINI_GNB_C_MAX_PATH];
  char prach_path[MINI_GNB_C_MAX_PATH];
  char msg3_path[MINI_GNB_C_MAX_PATH];
  char pucch_sr_path[MINI_GNB_C_MAX_PATH];
  char bsr_path[MINI_GNB_C_MAX_PATH];
  char ul_data_path[MINI_GNB_C_MAX_PATH];
  char tx_dir[MINI_GNB_C_MAX_PATH];
  char msg4_tx_path[MINI_GNB_C_MAX_PATH];
  char msg4_pdcch_path[MINI_GNB_C_MAX_PATH];
  char ssb_tx_path[MINI_GNB_C_MAX_PATH];
  char dl_data_tx_path[MINI_GNB_C_MAX_PATH];
  char dl_data_pdcch_path[MINI_GNB_C_MAX_PATH];
  char bsr_grant_pdcch_path[MINI_GNB_C_MAX_PATH];
  char data_grant_pdcch_path[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  char contention_id_hex[MINI_GNB_C_MAX_TEXT];
  mini_gnb_c_config_t config;
  mini_gnb_c_simulator_t simulator;
  mini_gnb_c_run_summary_t summary;
  char* msg4_tx_text = NULL;
  char* msg4_pdcch_text = NULL;
  char* ssb_tx_text = NULL;
  char* dl_data_tx_text = NULL;
  char* dl_data_pdcch_text = NULL;
  char* bsr_grant_pdcch_text = NULL;
  char* data_grant_pdcch_text = NULL;

  mini_gnb_c_default_config_path(config_path, sizeof(config_path));
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) == 0,
                     "expected config to load");
  mini_gnb_c_disable_local_exchange(&config);

  config.sim.total_slots = 40;
  config.sim.msg3_present = false;
  config.sim.prach_retry_delay_slots = -1;
  config.broadcast.prach_period_slots = 1;
  config.broadcast.prach_offset_slot = 0;
  config.sim.post_msg4_traffic_enabled = true;
  config.sim.post_msg4_dl_pdcch_delay_slots = 1;
  config.sim.post_msg4_dl_time_indicator = 1;
  config.sim.post_msg4_dl_data_to_ul_ack_slots = 1;
  config.sim.post_msg4_ul_grant_delay_slots = 1;
  config.sim.post_msg4_ul_time_indicator = 2;
  config.sim.ul_data_present = false;
  config.sim.ul_bsr_buffer_size_bytes = 384;

  mini_gnb_c_make_output_dir("test_integration_slot_text_c", output_dir, sizeof(output_dir));
  mini_gnb_c_require(mini_gnb_c_join_path(output_dir, "input", input_dir, sizeof(input_dir)) == 0,
                     "expected input directory path");
  mini_gnb_c_reset_test_dir(input_dir);
  mini_gnb_c_require(snprintf(config.sim.ul_input_dir,
                              sizeof(config.sim.ul_input_dir),
                              "%s",
                              input_dir) < (int)sizeof(config.sim.ul_input_dir),
                     "expected input directory to fit in config");
  mini_gnb_c_require(snprintf(prach_path,
                              sizeof(prach_path),
                              "%s/slot_20_UL_OBJ_PRACH.txt",
                              input_dir) < (int)sizeof(prach_path),
                     "expected PRACH text path");
  mini_gnb_c_require(snprintf(msg3_path,
                              sizeof(msg3_path),
                              "%s/slot_24_UL_OBJ_MSG3.txt",
                              input_dir) < (int)sizeof(msg3_path),
                     "expected Msg3 text path");
  mini_gnb_c_require(snprintf(pucch_sr_path,
                              sizeof(pucch_sr_path),
                              "%s/slot_32_UL_OBJ_PUCCH_SR.txt",
                              input_dir) < (int)sizeof(pucch_sr_path),
                     "expected PUCCH SR text path");
  mini_gnb_c_require(snprintf(bsr_path,
                              sizeof(bsr_path),
                              "%s/slot_35_UL_OBJ_DATA.txt",
                              input_dir) < (int)sizeof(bsr_path),
                     "expected UL BSR text path");
  mini_gnb_c_require(snprintf(ul_data_path,
                              sizeof(ul_data_path),
                              "%s/slot_38_UL_OBJ_DATA.txt",
                              input_dir) < (int)sizeof(ul_data_path),
                     "expected UL data text path");

  mini_gnb_c_write_transport_text_file(prach_path,
                                       "direction=UL\n"
                                       "abs_slot=20\n"
                                       "type=UL_OBJ_PRACH\n"
                                       "preamble_id=31\n"
                                       "ta_est=7\n"
                                       "peak_metric=14.5\n"
                                       "sample_count=64\n");
  mini_gnb_c_write_transport_text_file(msg3_path,
                                       "direction=UL\n"
                                       "abs_slot=24\n"
                                       "type=UL_OBJ_MSG3\n"
                                       "rnti=17921\n"
                                       "snr_db=17.5\n"
                                       "evm=1.7\n"
                                       "crc_ok=true\n"
                                       "sample_count=96\n"
                                       "payload_hex=020201460110DEADBEEFCAFE05020102030405060708\n");
  mini_gnb_c_write_transport_text_file(pucch_sr_path,
                                       "direction=UL\n"
                                       "abs_slot=32\n"
                                       "type=UL_OBJ_PUCCH_SR\n"
                                       "rnti=17921\n"
                                       "snr_db=13.2\n"
                                       "crc_ok=true\n"
                                       "sample_count=72\n");
  mini_gnb_c_write_transport_text_file(bsr_path,
                                       "direction=UL\n"
                                       "abs_slot=35\n"
                                       "type=UL_OBJ_DATA\n"
                                       "rnti=17921\n"
                                       "snr_db=14.4\n"
                                       "evm=1.3\n"
                                       "crc_ok=true\n"
                                       "sample_count=96\n"
                                       "tbsize=16\n"
                                       "payload_text=BSR|bytes=384\n");
  mini_gnb_c_write_transport_text_file(ul_data_path,
                                       "direction=UL\n"
                                       "abs_slot=38\n"
                                       "type=UL_OBJ_DATA\n"
                                       "rnti=17921\n"
                                       "snr_db=15.1\n"
                                       "evm=2.4\n"
                                       "crc_ok=true\n"
                                       "sample_count=128\n"
                                       "tbsize=96\n"
                                       "payload_text=UL_DATA_TEST\n");

  mini_gnb_c_simulator_init(&simulator, &config, output_dir);
  mini_gnb_c_require(mini_gnb_c_simulator_run(&simulator, &summary) == 0, "expected simulator run");

  mini_gnb_c_require(summary.counters.prach_detect_ok == 1U, "expected text-driven PRACH detection");
  mini_gnb_c_require(summary.counters.rar_sent == 1U, "expected RAR after text-driven PRACH");
  mini_gnb_c_require(summary.counters.msg3_crc_ok == 1U, "expected Msg3 decode from text transport");
  mini_gnb_c_require(summary.counters.rrcsetup_sent == 1U, "expected Msg4 after text-driven Msg3");
  mini_gnb_c_require(summary.counters.pucch_sr_detect_ok == 1U, "expected one text-driven PUCCH SR");
  mini_gnb_c_require(summary.counters.ul_bsr_rx_ok == 1U, "expected one scheduled UL BSR");
  mini_gnb_c_require(summary.counters.dl_data_sent >= 1U,
                     "expected at least one scheduled PUCCH config DL burst");
  mini_gnb_c_require(summary.counters.ul_data_rx_ok == 1U, "expected one scheduled UL data burst");
  mini_gnb_c_require(summary.has_ra_context, "expected RA context after text transport");
  mini_gnb_c_require(summary.ra_context.detect_abs_slot == 20, "expected text PRACH at abs_slot 20");
  mini_gnb_c_require(summary.ra_context.preamble_id == 31U, "expected preamble id taken from text transport");
  mini_gnb_c_require(summary.ra_context.ta_est == 7, "expected ta_est taken from text transport");
  mini_gnb_c_require(summary.ra_context.state == MINI_GNB_C_RA_DONE, "expected RA completion from text transport");
  mini_gnb_c_require(summary.ue_count == 1U, "expected one promoted UE context");
  mini_gnb_c_require(summary.ue_contexts[0].traffic_plan_scheduled, "expected post-Msg4 traffic plan scheduling");
  mini_gnb_c_require(summary.ue_contexts[0].dl_data_sent, "expected UE context DL config marker");
  mini_gnb_c_require(summary.ue_contexts[0].pucch_sr_detected, "expected UE context PUCCH SR marker");
  mini_gnb_c_require(summary.ue_contexts[0].ul_bsr_received, "expected UE context UL BSR marker");
  mini_gnb_c_require(summary.ue_contexts[0].ul_data_received, "expected UE context UL data marker");
  mini_gnb_c_require(summary.ue_contexts[0].dl_data_abs_slot == 27, "expected DL data slot after Msg4");
  mini_gnb_c_require(summary.ue_contexts[0].pucch_sr_abs_slot == 32, "expected PUCCH SR at abs_slot 32");
  mini_gnb_c_require(summary.ue_contexts[0].small_ul_grant_abs_slot == 35,
                     "expected compact BSR grant to land at abs_slot 35");
  mini_gnb_c_require(summary.ue_contexts[0].ul_bsr_abs_slot == 35, "expected BSR on the small grant");
  mini_gnb_c_require(summary.ue_contexts[0].large_ul_grant_abs_slot == 38,
                     "expected large UL grant to land at abs_slot 38");
  mini_gnb_c_require(summary.ue_contexts[0].ul_data_abs_slot == 38, "expected UL data slot after large grant");
  mini_gnb_c_require(mini_gnb_c_bytes_to_hex(summary.ra_context.contention_id48,
                                             6U,
                                             contention_id_hex,
                                             sizeof(contention_id_hex)) == 0,
                     "expected contention identity serialization");
  mini_gnb_c_require(strcmp(contention_id_hex, "DEADBEEFCAFE") == 0,
                     "expected contention identity to come from Msg3 text file");

  mini_gnb_c_require(mini_gnb_c_join_path(output_dir, "tx", tx_dir, sizeof(tx_dir)) == 0,
                     "expected tx directory path");
  mini_gnb_c_require(snprintf(msg4_tx_path,
                              sizeof(msg4_tx_path),
                              "%s/slot_25_DL_OBJ_MSG4.txt",
                              tx_dir) < (int)sizeof(msg4_tx_path),
                     "expected Msg4 transport text path");
  msg4_tx_text = mini_gnb_c_read_text_file(msg4_tx_path);
  mini_gnb_c_require(msg4_tx_text != NULL, "expected Msg4 text transport export");
  mini_gnb_c_require(strstr(msg4_tx_text, "type=DL_OBJ_MSG4") != NULL, "expected Msg4 type in text export");
  mini_gnb_c_require(strstr(msg4_tx_text, "payload_hex=") != NULL, "expected payload hex in text export");
  mini_gnb_c_require(strstr(msg4_tx_text, "scheduled_by_pdcch=true") != NULL,
                     "expected Msg4 payload export to show PDCCH scheduling");

  mini_gnb_c_require(snprintf(msg4_pdcch_path,
                              sizeof(msg4_pdcch_path),
                              "%s/slot_25_DL_OBJ_PDCCH_DCI1_0_rnti_17921_MSG4.txt",
                              tx_dir) < (int)sizeof(msg4_pdcch_path),
                     "expected Msg4 PDCCH transport text path");
  msg4_pdcch_text = mini_gnb_c_read_text_file(msg4_pdcch_path);
  mini_gnb_c_require(msg4_pdcch_text != NULL, "expected Msg4 PDCCH text export");
  mini_gnb_c_require(strstr(msg4_pdcch_text, "channel=PDCCH") != NULL, "expected PDCCH channel export");
  mini_gnb_c_require(strstr(msg4_pdcch_text, "dci_format=DCI1_0") != NULL, "expected DCI1_0 export");
  mini_gnb_c_require(strstr(msg4_pdcch_text, "scheduled_type=MSG4") != NULL,
                     "expected PDCCH to point at Msg4");

  mini_gnb_c_require(snprintf(ssb_tx_path,
                              sizeof(ssb_tx_path),
                              "%s/slot_20_DL_OBJ_SSB.txt",
                              tx_dir) < (int)sizeof(ssb_tx_path),
                     "expected SSB text path");
  ssb_tx_text = mini_gnb_c_read_text_file(ssb_tx_path);
  mini_gnb_c_require(ssb_tx_text != NULL, "expected SSB text export");
  mini_gnb_c_require(strstr(ssb_tx_text, "channel=PBCH") != NULL, "expected SSB to export as PBCH");
  mini_gnb_c_require(strstr(ssb_tx_text, "scheduled_by_pdcch=false") != NULL,
                     "expected SSB to remain outside PDCCH scheduling");

  mini_gnb_c_require(snprintf(dl_data_tx_path,
                              sizeof(dl_data_tx_path),
                              "%s/slot_27_DL_OBJ_DATA.txt",
                              tx_dir) < (int)sizeof(dl_data_tx_path),
                     "expected DL data text path");
  dl_data_tx_text = mini_gnb_c_read_text_file(dl_data_tx_path);
  mini_gnb_c_require(dl_data_tx_text != NULL, "expected DL data text export");
  mini_gnb_c_require(strstr(dl_data_tx_text, "type=DL_OBJ_DATA") != NULL, "expected DL data type export");
  mini_gnb_c_require(strstr(dl_data_tx_text, "dci_format=DCI1_1") != NULL, "expected DCI1_1 for DL data");
  mini_gnb_c_require(strstr(dl_data_tx_text, "tbsize=120") != NULL, "expected DL data tbsize export");
  mini_gnb_c_require(strstr(dl_data_tx_text, "payload_text=PUCCH_CFG|sr_abs_slot=32") != NULL,
                     "expected DL data payload to carry PUCCH config");

  mini_gnb_c_require(snprintf(dl_data_pdcch_path,
                              sizeof(dl_data_pdcch_path),
                              "%s/slot_26_DL_OBJ_PDCCH_DCI1_1_rnti_17921_DATA.txt",
                              tx_dir) < (int)sizeof(dl_data_pdcch_path),
                     "expected DL data PDCCH path");
  dl_data_pdcch_text = mini_gnb_c_read_text_file(dl_data_pdcch_path);
  mini_gnb_c_require(dl_data_pdcch_text != NULL, "expected DL data PDCCH export");
  mini_gnb_c_require(strstr(dl_data_pdcch_text, "dci_format=DCI1_1") != NULL,
                     "expected DCI1_1 control export");
  mini_gnb_c_require(strstr(dl_data_pdcch_text, "scheduled_abs_slot=27") != NULL,
                     "expected DCI1_1 to point at the PUCCH config PDSCH");
  mini_gnb_c_require(strstr(dl_data_pdcch_text, "time_indicator=1") != NULL,
                     "expected DL time indicator in PDCCH export");
  mini_gnb_c_require(strstr(dl_data_pdcch_text, "dl_data_to_ul_ack=1") != NULL,
                     "expected DL ACK timing in PDCCH export");
  mini_gnb_c_require(strstr(dl_data_pdcch_text, "harq_id=0") != NULL,
                     "expected DL HARQ ID in PDCCH export");
  mini_gnb_c_require(strstr(dl_data_pdcch_text, "is_new_data=true") != NULL,
                     "expected DL new-data flag in PDCCH export");

  mini_gnb_c_require(snprintf(bsr_grant_pdcch_path,
                              sizeof(bsr_grant_pdcch_path),
                              "%s/slot_33_DL_OBJ_PDCCH_DCI0_1_rnti_17921_BSR.txt",
                              tx_dir) < (int)sizeof(bsr_grant_pdcch_path),
                     "expected compact BSR grant PDCCH path");
  bsr_grant_pdcch_text = mini_gnb_c_read_text_file(bsr_grant_pdcch_path);
  mini_gnb_c_require(bsr_grant_pdcch_text != NULL, "expected compact BSR grant PDCCH export");
  mini_gnb_c_require(strstr(bsr_grant_pdcch_text, "dci_format=DCI0_1") != NULL,
                     "expected DCI0_1 control export for BSR");
  mini_gnb_c_require(strstr(bsr_grant_pdcch_text, "scheduled_type=BSR") != NULL,
                     "expected first UL grant to target BSR");
  mini_gnb_c_require(strstr(bsr_grant_pdcch_text, "scheduled_purpose=BSR") != NULL,
                     "expected first UL grant purpose to be BSR");
  mini_gnb_c_require(strstr(bsr_grant_pdcch_text, "tbsize=16") != NULL,
                     "expected compact BSR grant tbsize export");
  mini_gnb_c_require(strstr(bsr_grant_pdcch_text, "scheduled_abs_slot=35") != NULL,
                     "expected compact grant to point at the BSR slot");
  mini_gnb_c_require(strstr(bsr_grant_pdcch_text, "time_indicator=2") != NULL,
                     "expected UL BSR time indicator in PDCCH export");
  mini_gnb_c_require(strstr(bsr_grant_pdcch_text, "harq_id=0") != NULL,
                     "expected UL BSR HARQ ID in PDCCH export");
  mini_gnb_c_require(strstr(bsr_grant_pdcch_text, "ndi=false") != NULL,
                     "expected first UL grant NDI to be false");

  mini_gnb_c_require(snprintf(data_grant_pdcch_path,
                              sizeof(data_grant_pdcch_path),
                              "%s/slot_36_DL_OBJ_PDCCH_DCI0_1_rnti_17921_DATA.txt",
                              tx_dir) < (int)sizeof(data_grant_pdcch_path),
                     "expected large UL data grant PDCCH path");
  data_grant_pdcch_text = mini_gnb_c_read_text_file(data_grant_pdcch_path);
  mini_gnb_c_require(data_grant_pdcch_text != NULL, "expected large UL data grant PDCCH export");
  mini_gnb_c_require(strstr(data_grant_pdcch_text, "dci_format=DCI0_1") != NULL,
                     "expected DCI0_1 control export for payload");
  mini_gnb_c_require(strstr(data_grant_pdcch_text, "scheduled_type=DATA") != NULL,
                     "expected second UL grant to target data");
  mini_gnb_c_require(strstr(data_grant_pdcch_text, "scheduled_purpose=DATA") != NULL,
                     "expected second UL grant purpose to be data");
  mini_gnb_c_require(strstr(data_grant_pdcch_text, "tbsize=96") != NULL,
                     "expected large UL grant tbsize export");
  mini_gnb_c_require(strstr(data_grant_pdcch_text, "scheduled_abs_slot=38") != NULL,
                     "expected large grant to point at the UL payload slot");
  mini_gnb_c_require(strstr(data_grant_pdcch_text, "time_indicator=2") != NULL,
                     "expected UL payload time indicator in PDCCH export");
  mini_gnb_c_require(strstr(data_grant_pdcch_text, "ndi=true") != NULL,
                     "expected second UL grant NDI to flip after BSR success");

  free(data_grant_pdcch_text);
  free(bsr_grant_pdcch_text);
  free(dl_data_pdcch_text);
  free(dl_data_tx_text);
  free(ssb_tx_text);
  free(msg4_pdcch_text);
  free(msg4_tx_text);
}

void test_integration_msg3_missing_retries_prach(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char output_dir[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  mini_gnb_c_config_t config;
  mini_gnb_c_simulator_t simulator;
  mini_gnb_c_run_summary_t summary;

  mini_gnb_c_default_config_path(config_path, sizeof(config_path));
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) == 0,
                     "expected config to load");
  mini_gnb_c_disable_local_exchange(&config);

  config.sim.msg3_present = false;
  config.sim.total_slots = 18;
  config.sim.prach_retry_delay_slots = 4;
  config.broadcast.prach_period_slots = 1;
  config.broadcast.prach_offset_slot = 0;

  mini_gnb_c_make_output_dir("test_integration_msg3_missing_c", output_dir, sizeof(output_dir));
  mini_gnb_c_simulator_init(&simulator, &config, output_dir);
  mini_gnb_c_require(mini_gnb_c_simulator_run(&simulator, &summary) == 0, "expected simulator run");

  mini_gnb_c_require(summary.counters.prach_detect_ok == 2U, "expected initial PRACH and retry PRACH");
  mini_gnb_c_require(summary.counters.rar_sent == 2U, "expected two RAR transmissions after retry");
  mini_gnb_c_require(summary.counters.msg3_crc_ok == 0U, "expected no Msg3 CRC success");
  mini_gnb_c_require(summary.counters.rrcsetup_sent == 0U, "expected no Msg4 when Msg3 is absent");
  mini_gnb_c_require(summary.counters.ra_timeout == 2U, "expected timeout for each missing Msg3 attempt");
  mini_gnb_c_require(summary.has_ra_context, "expected final RA context to remain visible in summary");
  mini_gnb_c_require(summary.ra_context.state == MINI_GNB_C_RA_FAIL, "expected final RA state to be FAIL");
  mini_gnb_c_require(summary.ra_context.detect_abs_slot == 10, "expected retry PRACH to become the final RA context");
  mini_gnb_c_require(summary.ue_count == 0U, "expected no UE promotion without Msg3");
}

void test_integration_msg3_rnti_mismatch_rejected_after_retry(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char output_dir[MINI_GNB_C_MAX_PATH];
  char input_dir[MINI_GNB_C_MAX_PATH];
  char prach_first_path[MINI_GNB_C_MAX_PATH];
  char prach_second_path[MINI_GNB_C_MAX_PATH];
  char msg3_second_path[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  mini_gnb_c_config_t config;
  mini_gnb_c_simulator_t simulator;
  mini_gnb_c_run_summary_t summary;

  mini_gnb_c_default_config_path(config_path, sizeof(config_path));
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) == 0,
                     "expected config to load");
  mini_gnb_c_disable_local_exchange(&config);

  config.sim.total_slots = 50;
  config.sim.msg3_present = false;
  config.sim.prach_retry_delay_slots = -1;
  config.broadcast.prach_period_slots = 1;
  config.broadcast.prach_offset_slot = 0;

  mini_gnb_c_make_output_dir("test_integration_msg3_rnti_mismatch_c", output_dir, sizeof(output_dir));
  mini_gnb_c_require(mini_gnb_c_join_path(output_dir, "input", input_dir, sizeof(input_dir)) == 0,
                     "expected input directory path");
  mini_gnb_c_require(snprintf(config.sim.ul_input_dir,
                              sizeof(config.sim.ul_input_dir),
                              "%s",
                              input_dir) < (int)sizeof(config.sim.ul_input_dir),
                     "expected input directory to fit in config");
  mini_gnb_c_require(snprintf(prach_first_path,
                              sizeof(prach_first_path),
                              "%s/slot_20_UL_OBJ_PRACH.txt",
                              input_dir) < (int)sizeof(prach_first_path),
                     "expected first PRACH path");
  mini_gnb_c_require(snprintf(prach_second_path,
                              sizeof(prach_second_path),
                              "%s/slot_40_UL_OBJ_PRACH.txt",
                              input_dir) < (int)sizeof(prach_second_path),
                     "expected second PRACH path");
  mini_gnb_c_require(snprintf(msg3_second_path,
                              sizeof(msg3_second_path),
                              "%s/slot_44_UL_OBJ_MSG3.txt",
                              input_dir) < (int)sizeof(msg3_second_path),
                     "expected second Msg3 path");

  mini_gnb_c_write_transport_text_file(prach_first_path,
                                       "direction=UL\n"
                                       "abs_slot=20\n"
                                       "type=UL_OBJ_PRACH\n"
                                       "preamble_id=27\n"
                                       "ta_est=11\n"
                                       "peak_metric=18.5\n"
                                       "sample_count=64\n");
  mini_gnb_c_write_transport_text_file(prach_second_path,
                                       "direction=UL\n"
                                       "abs_slot=40\n"
                                       "type=UL_OBJ_PRACH\n"
                                       "preamble_id=28\n"
                                       "ta_est=9\n"
                                       "peak_metric=17.0\n"
                                       "sample_count=64\n");
  mini_gnb_c_write_transport_text_file(msg3_second_path,
                                       "direction=UL\n"
                                       "abs_slot=44\n"
                                       "type=UL_OBJ_MSG3\n"
                                       "rnti=17921\n"
                                       "snr_db=18.2\n"
                                       "evm=2.1\n"
                                       "crc_ok=true\n"
                                       "sample_count=96\n"
                                       "payload_hex=020201460110A1B2C3D4E5F603011122334455667788\n");

  mini_gnb_c_simulator_init(&simulator, &config, output_dir);
  mini_gnb_c_require(mini_gnb_c_simulator_run(&simulator, &summary) == 0, "expected simulator run");

  mini_gnb_c_require(summary.counters.prach_detect_ok == 2U, "expected both PRACH attempts to be detected");
  mini_gnb_c_require(summary.counters.rar_sent == 2U, "expected one RAR per PRACH attempt");
  mini_gnb_c_require(summary.counters.msg3_crc_ok == 0U,
                     "expected mismatched second-attempt Msg3 to be rejected before decode success");
  mini_gnb_c_require(summary.counters.rrcsetup_sent == 0U, "expected no Msg4 after Msg3 RNTI mismatch");
  mini_gnb_c_require(summary.counters.ra_timeout == 2U,
                     "expected each attempt to end in timeout when no valid Msg3 arrives");
  mini_gnb_c_require(summary.has_ra_context, "expected final RA context to remain visible in summary");
  mini_gnb_c_require(summary.ra_context.detect_abs_slot == 40, "expected second PRACH to become the final RA context");
  mini_gnb_c_require(summary.ra_context.tc_rnti == 17922U,
                     "expected second PRACH to allocate a new TC-RNTI");
  mini_gnb_c_require(summary.ra_context.state == MINI_GNB_C_RA_FAIL,
                     "expected final RA state to fail after mismatched Msg3");
  mini_gnb_c_require(strcmp(summary.ra_context.last_failure, "MSG3_TIMEOUT") == 0,
                     "expected mismatched Msg3 to leave the RA attempt waiting until timeout");
  mini_gnb_c_require(summary.ue_count == 0U, "expected no UE promotion when Msg3 RNTI mismatches");
}

void test_integration_scripted_schedule_files(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char output_dir[MINI_GNB_C_MAX_PATH];
  char input_dir[MINI_GNB_C_MAX_PATH];
  char schedule_dir[MINI_GNB_C_MAX_PATH];
  char prach_path[MINI_GNB_C_MAX_PATH];
  char msg3_path[MINI_GNB_C_MAX_PATH];
  char bsr_path[MINI_GNB_C_MAX_PATH];
  char ul_data_path[MINI_GNB_C_MAX_PATH];
  char dl_schedule_path[MINI_GNB_C_MAX_PATH];
  char bsr_schedule_path[MINI_GNB_C_MAX_PATH];
  char data_schedule_path[MINI_GNB_C_MAX_PATH];
  char tx_dir[MINI_GNB_C_MAX_PATH];
  char dl_data_tx_path[MINI_GNB_C_MAX_PATH];
  char dl_data_pdcch_path[MINI_GNB_C_MAX_PATH];
  char bsr_grant_pdcch_path[MINI_GNB_C_MAX_PATH];
  char data_grant_pdcch_path[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  mini_gnb_c_config_t config;
  mini_gnb_c_simulator_t simulator;
  mini_gnb_c_run_summary_t summary;
  char* dl_data_tx_text = NULL;
  char* dl_data_pdcch_text = NULL;
  char* bsr_grant_pdcch_text = NULL;
  char* data_grant_pdcch_text = NULL;

  mini_gnb_c_default_config_path(config_path, sizeof(config_path));
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) == 0,
                     "expected config to load");
  mini_gnb_c_disable_local_exchange(&config);

  config.sim.total_slots = 45;
  config.sim.msg3_present = false;
  config.sim.post_msg4_traffic_enabled = false;
  config.broadcast.prach_period_slots = 1;
  config.broadcast.prach_offset_slot = 0;
  config.sim.ul_data_present = false;

  mini_gnb_c_make_output_dir("test_integration_scripted_schedule_c", output_dir, sizeof(output_dir));
  mini_gnb_c_require(mini_gnb_c_join_path(output_dir, "scripted_schedule_input", input_dir, sizeof(input_dir)) == 0,
                     "expected scripted input directory");
  mini_gnb_c_require(mini_gnb_c_join_path(output_dir, "scripted_schedule_plan", schedule_dir, sizeof(schedule_dir)) ==
                         0,
                     "expected scripted schedule directory");
  mini_gnb_c_reset_test_dir(input_dir);
  mini_gnb_c_reset_test_dir(schedule_dir);
  mini_gnb_c_require(snprintf(config.sim.ul_input_dir, sizeof(config.sim.ul_input_dir), "%s", input_dir) <
                         (int)sizeof(config.sim.ul_input_dir),
                     "expected input directory config");
  mini_gnb_c_require(snprintf(config.sim.scripted_schedule_dir,
                              sizeof(config.sim.scripted_schedule_dir),
                              "%s",
                              schedule_dir) < (int)sizeof(config.sim.scripted_schedule_dir),
                     "expected scripted schedule directory config");

  mini_gnb_c_require(snprintf(prach_path, sizeof(prach_path), "%s/slot_20_UL_OBJ_PRACH.txt", input_dir) <
                         (int)sizeof(prach_path),
                     "expected PRACH path");
  mini_gnb_c_require(snprintf(msg3_path, sizeof(msg3_path), "%s/slot_24_UL_OBJ_MSG3.txt", input_dir) <
                         (int)sizeof(msg3_path),
                     "expected Msg3 path");
  mini_gnb_c_require(snprintf(bsr_path, sizeof(bsr_path), "%s/slot_33_UL_OBJ_DATA.txt", input_dir) <
                         (int)sizeof(bsr_path),
                     "expected BSR path");
  mini_gnb_c_require(snprintf(ul_data_path, sizeof(ul_data_path), "%s/slot_36_UL_OBJ_DATA.txt", input_dir) <
                         (int)sizeof(ul_data_path),
                     "expected UL data path");
  mini_gnb_c_require(snprintf(dl_schedule_path, sizeof(dl_schedule_path), "%s/slot_27_SCRIPT_DL.txt", schedule_dir) <
                         (int)sizeof(dl_schedule_path),
                     "expected scripted DL path");
  mini_gnb_c_require(snprintf(bsr_schedule_path, sizeof(bsr_schedule_path), "%s/slot_31_SCRIPT_UL.txt", schedule_dir) <
                         (int)sizeof(bsr_schedule_path),
                     "expected scripted BSR path");
  mini_gnb_c_require(snprintf(data_schedule_path,
                              sizeof(data_schedule_path),
                              "%s/slot_34_SCRIPT_UL.txt",
                              schedule_dir) < (int)sizeof(data_schedule_path),
                     "expected scripted UL data path");

  mini_gnb_c_write_transport_text_file(prach_path,
                                       "direction=UL\n"
                                       "abs_slot=20\n"
                                       "type=UL_OBJ_PRACH\n"
                                       "preamble_id=27\n"
                                       "ta_est=11\n"
                                       "peak_metric=18.5\n"
                                       "sample_count=64\n");
  mini_gnb_c_write_transport_text_file(msg3_path,
                                       "direction=UL\n"
                                       "abs_slot=24\n"
                                       "type=UL_OBJ_MSG3\n"
                                       "rnti=17921\n"
                                       "snr_db=18.2\n"
                                       "evm=2.1\n"
                                       "crc_ok=true\n"
                                       "sample_count=96\n"
                                       "payload_hex=020201460110A1B2C3D4E5F603011122334455667788\n");
  mini_gnb_c_write_transport_text_file(bsr_path,
                                       "direction=UL\n"
                                       "abs_slot=33\n"
                                       "type=UL_OBJ_DATA\n"
                                       "rnti=17921\n"
                                       "snr_db=14.0\n"
                                       "evm=1.2\n"
                                       "crc_ok=true\n"
                                       "sample_count=96\n"
                                       "tbsize=24\n"
                                       "payload_text=BSR|bytes=640\n");
  mini_gnb_c_write_transport_text_file(ul_data_path,
                                       "direction=UL\n"
                                       "abs_slot=36\n"
                                       "type=UL_OBJ_DATA\n"
                                       "rnti=17921\n"
                                       "snr_db=15.0\n"
                                       "evm=1.8\n"
                                       "crc_ok=true\n"
                                       "sample_count=128\n"
                                       "tbsize=64\n"
                                       "payload_text=SCRIPTED_UL_DATA\n");

  mini_gnb_c_write_transport_text_file(dl_schedule_path,
                                       "type=SCRIPT_DL_DATA\n"
                                       "abs_slot=27\n"
                                       "rnti=17921\n"
                                       "dci_format=DCI1_1\n"
                                       "prb_start=40\n"
                                       "prb_len=20\n"
                                       "mcs=8\n"
                                       "payload_text=SCRIPTED_DIRECT_DL\n");
  mini_gnb_c_write_transport_text_file(bsr_schedule_path,
                                       "type=SCRIPT_UL_GRANT\n"
                                       "rnti=17921\n"
                                       "scheduled_abs_slot=33\n"
                                       "purpose=BSR\n"
                                       "prb_start=58\n"
                                       "prb_len=12\n"
                                       "mcs=4\n"
                                       "k2=2\n");
  mini_gnb_c_write_transport_text_file(data_schedule_path,
                                       "type=SCRIPT_UL_GRANT\n"
                                       "rnti=17921\n"
                                       "scheduled_abs_slot=36\n"
                                       "purpose=DATA\n"
                                       "prb_start=46\n"
                                       "prb_len=16\n"
                                       "mcs=8\n"
                                       "k2=2\n");

  mini_gnb_c_simulator_init(&simulator, &config, output_dir);
  mini_gnb_c_require(mini_gnb_c_simulator_run(&simulator, &summary) == 0, "expected simulator run");

  mini_gnb_c_require(summary.counters.prach_detect_ok == 1U, "expected PRACH detection");
  mini_gnb_c_require(summary.counters.msg3_crc_ok == 1U, "expected Msg3 success");
  mini_gnb_c_require(summary.counters.rrcsetup_sent == 1U, "expected Msg4 send");
  mini_gnb_c_require(summary.counters.pucch_sr_detect_ok == 0U, "expected no scripted PUCCH SR");
  mini_gnb_c_require(summary.counters.dl_data_sent == 1U, "expected one scripted DL data send");
  mini_gnb_c_require(summary.counters.ul_bsr_rx_ok == 1U, "expected one scripted BSR receive");
  mini_gnb_c_require(summary.counters.ul_data_rx_ok == 1U, "expected one scripted UL data receive");
  mini_gnb_c_require(summary.ue_count == 1U, "expected one UE context");
  mini_gnb_c_require(summary.ue_contexts[0].dl_data_abs_slot == 27, "expected scripted DL abs slot");
  mini_gnb_c_require(summary.ue_contexts[0].ul_bsr_abs_slot == 33, "expected scripted BSR abs slot");
  mini_gnb_c_require(summary.ue_contexts[0].large_ul_grant_abs_slot == 36, "expected scripted UL data grant slot");
  mini_gnb_c_require(summary.ue_contexts[0].ul_data_abs_slot == 36, "expected scripted UL data abs slot");

  mini_gnb_c_require(mini_gnb_c_join_path(output_dir, "tx", tx_dir, sizeof(tx_dir)) == 0, "expected tx dir");
  mini_gnb_c_require(snprintf(dl_data_tx_path, sizeof(dl_data_tx_path), "%s/slot_27_DL_OBJ_DATA.txt", tx_dir) <
                         (int)sizeof(dl_data_tx_path),
                     "expected scripted DL tx path");
  mini_gnb_c_require(snprintf(dl_data_pdcch_path,
                              sizeof(dl_data_pdcch_path),
                              "%s/slot_27_DL_OBJ_PDCCH_DCI1_1_rnti_17921_DATA.txt",
                              tx_dir) < (int)sizeof(dl_data_pdcch_path),
                     "expected scripted DL pdcch path");
  mini_gnb_c_require(snprintf(bsr_grant_pdcch_path,
                              sizeof(bsr_grant_pdcch_path),
                              "%s/slot_31_DL_OBJ_PDCCH_DCI0_1_rnti_17921_BSR.txt",
                              tx_dir) < (int)sizeof(bsr_grant_pdcch_path),
                     "expected scripted BSR pdcch path");
  mini_gnb_c_require(snprintf(data_grant_pdcch_path,
                              sizeof(data_grant_pdcch_path),
                              "%s/slot_34_DL_OBJ_PDCCH_DCI0_1_rnti_17921_DATA.txt",
                              tx_dir) < (int)sizeof(data_grant_pdcch_path),
                     "expected scripted data pdcch path");

  dl_data_tx_text = mini_gnb_c_read_text_file(dl_data_tx_path);
  dl_data_pdcch_text = mini_gnb_c_read_text_file(dl_data_pdcch_path);
  bsr_grant_pdcch_text = mini_gnb_c_read_text_file(bsr_grant_pdcch_path);
  data_grant_pdcch_text = mini_gnb_c_read_text_file(data_grant_pdcch_path);

  mini_gnb_c_require(dl_data_tx_text != NULL, "expected scripted DL tx export");
  mini_gnb_c_require(dl_data_pdcch_text != NULL, "expected scripted DL pdcch export");
  mini_gnb_c_require(bsr_grant_pdcch_text != NULL, "expected scripted BSR pdcch export");
  mini_gnb_c_require(data_grant_pdcch_text != NULL, "expected scripted data pdcch export");
  mini_gnb_c_require(strstr(dl_data_tx_text, "payload_text=SCRIPTED_DIRECT_DL") != NULL,
                     "expected scripted DL payload");
  mini_gnb_c_require(strstr(dl_data_tx_text, "tbsize=80") != NULL, "expected scripted DL tbsize");
  mini_gnb_c_require(strstr(dl_data_pdcch_text, "scheduled_prb_len=20") != NULL, "expected scripted DL prb len");
  mini_gnb_c_require(strstr(dl_data_pdcch_text, "mcs=8") != NULL, "expected scripted DL mcs");
  mini_gnb_c_require(strstr(dl_data_pdcch_text, "tbsize=80") != NULL, "expected scripted DL tbsize in pdcch");
  mini_gnb_c_require(strstr(bsr_grant_pdcch_text, "scheduled_prb_len=12") != NULL,
                     "expected scripted BSR prb len");
  mini_gnb_c_require(strstr(bsr_grant_pdcch_text, "tbsize=24") != NULL, "expected scripted BSR tbsize");
  mini_gnb_c_require(strstr(data_grant_pdcch_text, "scheduled_prb_len=16") != NULL,
                     "expected scripted UL data prb len");
  mini_gnb_c_require(strstr(data_grant_pdcch_text, "tbsize=64") != NULL, "expected scripted UL data tbsize");

  free(dl_data_tx_text);
  free(dl_data_pdcch_text);
  free(bsr_grant_pdcch_text);
  free(data_grant_pdcch_text);
}

void test_integration_scripted_pdcch_files(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char output_dir[MINI_GNB_C_MAX_PATH];
  char input_dir[MINI_GNB_C_MAX_PATH];
  char pdcch_dir[MINI_GNB_C_MAX_PATH];
  char prach_path[MINI_GNB_C_MAX_PATH];
  char msg3_path[MINI_GNB_C_MAX_PATH];
  char bsr_path[MINI_GNB_C_MAX_PATH];
  char ul_data_path[MINI_GNB_C_MAX_PATH];
  char dl_pdcch_script_path[MINI_GNB_C_MAX_PATH];
  char bsr_pdcch_script_path[MINI_GNB_C_MAX_PATH];
  char data_pdcch_script_path[MINI_GNB_C_MAX_PATH];
  char tx_dir[MINI_GNB_C_MAX_PATH];
  char dl_data_tx_path[MINI_GNB_C_MAX_PATH];
  char dl_data_pdcch_path[MINI_GNB_C_MAX_PATH];
  char bsr_grant_pdcch_path[MINI_GNB_C_MAX_PATH];
  char data_grant_pdcch_path[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  mini_gnb_c_config_t config;
  mini_gnb_c_simulator_t simulator;
  mini_gnb_c_run_summary_t summary;
  char* dl_data_tx_text = NULL;
  char* dl_data_pdcch_text = NULL;
  char* bsr_grant_pdcch_text = NULL;
  char* data_grant_pdcch_text = NULL;

  mini_gnb_c_default_config_path(config_path, sizeof(config_path));
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) == 0,
                     "expected config to load");
  mini_gnb_c_disable_local_exchange(&config);

  config.sim.total_slots = 45;
  config.sim.msg3_present = false;
  config.sim.post_msg4_traffic_enabled = false;
  config.broadcast.prach_period_slots = 1;
  config.broadcast.prach_offset_slot = 0;
  config.sim.ul_data_present = false;

  mini_gnb_c_make_output_dir("test_integration_scripted_pdcch_c", output_dir, sizeof(output_dir));
  mini_gnb_c_require(mini_gnb_c_join_path(output_dir, "scripted_pdcch_input", input_dir, sizeof(input_dir)) == 0,
                     "expected scripted pdcch input dir");
  mini_gnb_c_require(mini_gnb_c_join_path(output_dir, "scripted_pdcch_plan", pdcch_dir, sizeof(pdcch_dir)) == 0,
                     "expected scripted pdcch plan dir");
  mini_gnb_c_reset_test_dir(input_dir);
  mini_gnb_c_reset_test_dir(pdcch_dir);
  mini_gnb_c_require(snprintf(config.sim.ul_input_dir, sizeof(config.sim.ul_input_dir), "%s", input_dir) <
                         (int)sizeof(config.sim.ul_input_dir),
                     "expected pdcch input dir config");
  mini_gnb_c_require(snprintf(config.sim.scripted_pdcch_dir, sizeof(config.sim.scripted_pdcch_dir), "%s", pdcch_dir) <
                         (int)sizeof(config.sim.scripted_pdcch_dir),
                     "expected scripted pdcch dir config");

  mini_gnb_c_require(snprintf(prach_path, sizeof(prach_path), "%s/slot_20_UL_OBJ_PRACH.txt", input_dir) <
                         (int)sizeof(prach_path),
                     "expected pdcch PRACH path");
  mini_gnb_c_require(snprintf(msg3_path, sizeof(msg3_path), "%s/slot_24_UL_OBJ_MSG3.txt", input_dir) <
                         (int)sizeof(msg3_path),
                     "expected pdcch Msg3 path");
  mini_gnb_c_require(snprintf(bsr_path, sizeof(bsr_path), "%s/slot_33_UL_OBJ_DATA.txt", input_dir) <
                         (int)sizeof(bsr_path),
                     "expected pdcch BSR path");
  mini_gnb_c_require(snprintf(ul_data_path, sizeof(ul_data_path), "%s/slot_36_UL_OBJ_DATA.txt", input_dir) <
                         (int)sizeof(ul_data_path),
                     "expected pdcch UL data path");
  mini_gnb_c_require(snprintf(dl_pdcch_script_path,
                              sizeof(dl_pdcch_script_path),
                              "%s/slot_27_SCRIPT_PDCCH_DL.txt",
                              pdcch_dir) < (int)sizeof(dl_pdcch_script_path),
                     "expected scripted pdcch DL path");
  mini_gnb_c_require(snprintf(bsr_pdcch_script_path,
                              sizeof(bsr_pdcch_script_path),
                              "%s/slot_31_SCRIPT_PDCCH_UL.txt",
                              pdcch_dir) < (int)sizeof(bsr_pdcch_script_path),
                     "expected scripted pdcch BSR path");
  mini_gnb_c_require(snprintf(data_pdcch_script_path,
                              sizeof(data_pdcch_script_path),
                              "%s/slot_34_SCRIPT_PDCCH_UL.txt",
                              pdcch_dir) < (int)sizeof(data_pdcch_script_path),
                     "expected scripted pdcch data path");

  mini_gnb_c_write_transport_text_file(prach_path,
                                       "direction=UL\n"
                                       "abs_slot=20\n"
                                       "type=UL_OBJ_PRACH\n"
                                       "preamble_id=27\n"
                                       "ta_est=11\n"
                                       "peak_metric=18.5\n"
                                       "sample_count=64\n");
  mini_gnb_c_write_transport_text_file(msg3_path,
                                       "direction=UL\n"
                                       "abs_slot=24\n"
                                       "type=UL_OBJ_MSG3\n"
                                       "rnti=17921\n"
                                       "snr_db=18.2\n"
                                       "evm=2.1\n"
                                       "crc_ok=true\n"
                                       "sample_count=96\n"
                                       "payload_hex=020201460110A1B2C3D4E5F603011122334455667788\n");
  mini_gnb_c_write_transport_text_file(bsr_path,
                                       "direction=UL\n"
                                       "abs_slot=33\n"
                                       "type=UL_OBJ_DATA\n"
                                       "rnti=17921\n"
                                       "snr_db=14.5\n"
                                       "evm=1.1\n"
                                       "crc_ok=true\n"
                                       "sample_count=96\n"
                                       "tbsize=16\n"
                                       "payload_text=BSR|bytes=384\n");
  mini_gnb_c_write_transport_text_file(ul_data_path,
                                       "direction=UL\n"
                                       "abs_slot=36\n"
                                       "type=UL_OBJ_DATA\n"
                                       "rnti=17921\n"
                                       "snr_db=15.2\n"
                                       "evm=1.7\n"
                                       "crc_ok=true\n"
                                       "sample_count=144\n"
                                       "tbsize=128\n"
                                       "payload_text=SCRIPTED_PDCCH_UL_DATA\n");

  mini_gnb_c_write_transport_text_file(dl_pdcch_script_path,
                                       "direction=DL\n"
                                       "channel=PDCCH\n"
                                       "type=DL_OBJ_PDCCH\n"
                                       "rnti=17921\n"
                                       "dci_format=DCI1_0\n"
                                       "scheduled_abs_slot=27\n"
                                       "scheduled_type=DATA\n"
                                       "scheduled_prb_start=50\n"
                                       "scheduled_prb_len=24\n"
                                       "mcs=4\n"
                                       "payload_text=SCRIPTED_PDCCH_DL\n");
  mini_gnb_c_write_transport_text_file(bsr_pdcch_script_path,
                                       "direction=DL\n"
                                       "channel=PDCCH\n"
                                       "type=DL_OBJ_PDCCH\n"
                                       "rnti=17921\n"
                                       "dci_format=DCI0_1\n"
                                       "scheduled_abs_slot=33\n"
                                       "scheduled_type=BSR\n"
                                       "scheduled_purpose=BSR\n"
                                       "scheduled_prb_start=60\n"
                                       "scheduled_prb_len=8\n"
                                       "mcs=4\n"
                                       "k2=2\n");
  mini_gnb_c_write_transport_text_file(data_pdcch_script_path,
                                       "direction=DL\n"
                                       "channel=PDCCH\n"
                                       "type=DL_OBJ_PDCCH\n"
                                       "rnti=17921\n"
                                       "dci_format=DCI0_1\n"
                                       "scheduled_abs_slot=36\n"
                                       "scheduled_type=DATA\n"
                                       "scheduled_purpose=DATA\n"
                                       "scheduled_prb_start=44\n"
                                       "scheduled_prb_len=32\n"
                                       "mcs=8\n"
                                       "k2=2\n");

  mini_gnb_c_simulator_init(&simulator, &config, output_dir);
  mini_gnb_c_require(mini_gnb_c_simulator_run(&simulator, &summary) == 0, "expected simulator run");

  mini_gnb_c_require(summary.counters.prach_detect_ok == 1U, "expected PRACH detection");
  mini_gnb_c_require(summary.counters.msg3_crc_ok == 1U, "expected Msg3 success");
  mini_gnb_c_require(summary.counters.rrcsetup_sent == 1U, "expected Msg4 send");
  mini_gnb_c_require(summary.counters.dl_data_sent == 1U, "expected one PDCCH-driven DL data send");
  mini_gnb_c_require(summary.counters.ul_bsr_rx_ok == 1U, "expected one PDCCH-driven BSR receive");
  mini_gnb_c_require(summary.counters.ul_data_rx_ok == 1U, "expected one PDCCH-driven UL data receive");
  mini_gnb_c_require(summary.ue_count == 1U, "expected one UE context");
  mini_gnb_c_require(summary.ue_contexts[0].dl_data_abs_slot == 27, "expected pdcch DL abs slot");
  mini_gnb_c_require(summary.ue_contexts[0].ul_bsr_abs_slot == 33, "expected pdcch BSR abs slot");
  mini_gnb_c_require(summary.ue_contexts[0].ul_data_abs_slot == 36, "expected pdcch UL data abs slot");

  mini_gnb_c_require(mini_gnb_c_join_path(output_dir, "tx", tx_dir, sizeof(tx_dir)) == 0, "expected tx dir");
  mini_gnb_c_require(snprintf(dl_data_tx_path, sizeof(dl_data_tx_path), "%s/slot_27_DL_OBJ_DATA.txt", tx_dir) <
                         (int)sizeof(dl_data_tx_path),
                     "expected pdcch DL tx path");
  mini_gnb_c_require(snprintf(dl_data_pdcch_path,
                              sizeof(dl_data_pdcch_path),
                              "%s/slot_27_DL_OBJ_PDCCH_DCI1_0_rnti_17921_DATA.txt",
                              tx_dir) < (int)sizeof(dl_data_pdcch_path),
                     "expected pdcch DL pdcch path");
  mini_gnb_c_require(snprintf(bsr_grant_pdcch_path,
                              sizeof(bsr_grant_pdcch_path),
                              "%s/slot_31_DL_OBJ_PDCCH_DCI0_1_rnti_17921_BSR.txt",
                              tx_dir) < (int)sizeof(bsr_grant_pdcch_path),
                     "expected pdcch BSR path");
  mini_gnb_c_require(snprintf(data_grant_pdcch_path,
                              sizeof(data_grant_pdcch_path),
                              "%s/slot_34_DL_OBJ_PDCCH_DCI0_1_rnti_17921_DATA.txt",
                              tx_dir) < (int)sizeof(data_grant_pdcch_path),
                     "expected pdcch data path");

  dl_data_tx_text = mini_gnb_c_read_text_file(dl_data_tx_path);
  dl_data_pdcch_text = mini_gnb_c_read_text_file(dl_data_pdcch_path);
  bsr_grant_pdcch_text = mini_gnb_c_read_text_file(bsr_grant_pdcch_path);
  data_grant_pdcch_text = mini_gnb_c_read_text_file(data_grant_pdcch_path);

  mini_gnb_c_require(dl_data_tx_text != NULL, "expected pdcch DL tx export");
  mini_gnb_c_require(dl_data_pdcch_text != NULL, "expected pdcch DL pdcch export");
  mini_gnb_c_require(bsr_grant_pdcch_text != NULL, "expected pdcch BSR export");
  mini_gnb_c_require(data_grant_pdcch_text != NULL, "expected pdcch data export");
  mini_gnb_c_require(strstr(dl_data_tx_text, "payload_text=SCRIPTED_PDCCH_DL") != NULL,
                     "expected pdcch DL payload");
  mini_gnb_c_require(strstr(dl_data_tx_text, "tbsize=48") != NULL, "expected pdcch DL tbsize");
  mini_gnb_c_require(strstr(dl_data_pdcch_text, "dci_format=DCI1_0") != NULL, "expected scripted DCI1_0");
  mini_gnb_c_require(strstr(dl_data_pdcch_text, "mcs=4") != NULL, "expected scripted DL mcs");
  mini_gnb_c_require(strstr(dl_data_pdcch_text, "tbsize=48") != NULL, "expected scripted DL tbsize in pdcch");
  mini_gnb_c_require(strstr(bsr_grant_pdcch_text, "tbsize=16") != NULL, "expected pdcch BSR tbsize");
  mini_gnb_c_require(strstr(data_grant_pdcch_text, "scheduled_prb_len=32") != NULL,
                     "expected pdcch UL data prb len");
  mini_gnb_c_require(strstr(data_grant_pdcch_text, "tbsize=128") != NULL, "expected pdcch UL data tbsize");

  free(dl_data_tx_text);
  free(dl_data_pdcch_text);
  free(bsr_grant_pdcch_text);
  free(data_grant_pdcch_text);
}
