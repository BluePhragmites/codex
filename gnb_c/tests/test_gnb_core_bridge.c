#include <stdlib.h>
#include <string.h>

#include "test_helpers.h"

#include "mini_gnb_c/common/json_utils.h"
#include "mini_gnb_c/core/gnb_core_bridge.h"
#include "mini_gnb_c/link/json_link.h"
#include "mini_gnb_c/ngap/ngap_runtime.h"

typedef struct {
  uint8_t responses[8][MINI_GNB_C_CORE_BRIDGE_MAX_MESSAGE];
  size_t response_lengths[8];
  size_t response_count;
  size_t response_index;
  uint8_t sent_messages[8][MINI_GNB_C_CORE_BRIDGE_MAX_MESSAGE];
  size_t sent_lengths[8];
  size_t sent_count;
  uint32_t last_timeout_ms;
  char last_amf_ip[MINI_GNB_C_MAX_TEXT];
  uint32_t last_amf_port;
} mini_gnb_c_fake_ngap_transport_t;

static int mini_gnb_c_fake_transport_connect(mini_gnb_c_ngap_transport_t* transport,
                                             const char* amf_ip,
                                             uint32_t amf_port,
                                             uint32_t timeout_ms) {
  mini_gnb_c_fake_ngap_transport_t* fake = NULL;

  mini_gnb_c_require(transport != NULL, "expected transport in fake connect");
  fake = (mini_gnb_c_fake_ngap_transport_t*)transport->user_data;
  mini_gnb_c_require(fake != NULL, "expected fake transport state");
  fake->last_timeout_ms = timeout_ms;
  fake->last_amf_port = amf_port;
  (void)snprintf(fake->last_amf_ip, sizeof(fake->last_amf_ip), "%s", amf_ip != NULL ? amf_ip : "");
  transport->socket_fd = 1;
  return 0;
}

static int mini_gnb_c_fake_transport_send(mini_gnb_c_ngap_transport_t* transport,
                                          const uint8_t* bytes,
                                          size_t length) {
  mini_gnb_c_fake_ngap_transport_t* fake = NULL;

  mini_gnb_c_require(transport != NULL, "expected transport in fake send");
  mini_gnb_c_require(bytes != NULL && length > 0u, "expected bytes in fake send");
  fake = (mini_gnb_c_fake_ngap_transport_t*)transport->user_data;
  mini_gnb_c_require(fake != NULL, "expected fake transport state");
  mini_gnb_c_require(fake->sent_count < 8u, "unexpected extra send");
  mini_gnb_c_require(length <= sizeof(fake->sent_messages[0]), "unexpected fake send length");
  memcpy(fake->sent_messages[fake->sent_count], bytes, length);
  fake->sent_lengths[fake->sent_count] = length;
  ++fake->sent_count;
  return 0;
}

static int mini_gnb_c_fake_transport_recv(mini_gnb_c_ngap_transport_t* transport,
                                          uint8_t* response,
                                          size_t response_capacity,
                                          size_t* response_length) {
  mini_gnb_c_fake_ngap_transport_t* fake = NULL;
  size_t length = 0u;

  mini_gnb_c_require(transport != NULL, "expected transport in fake recv");
  mini_gnb_c_require(response != NULL && response_length != NULL, "expected fake recv buffers");
  fake = (mini_gnb_c_fake_ngap_transport_t*)transport->user_data;
  mini_gnb_c_require(fake != NULL, "expected fake transport state");
  mini_gnb_c_require(fake->response_index < fake->response_count, "unexpected extra recv");

  length = fake->response_lengths[fake->response_index];
  mini_gnb_c_require(length <= response_capacity, "expected enough recv capacity");
  memcpy(response, fake->responses[fake->response_index], length);
  *response_length = length;
  ++fake->response_index;
  return 0;
}

static void mini_gnb_c_fake_transport_close(mini_gnb_c_ngap_transport_t* transport) {
  if (transport != NULL) {
    transport->socket_fd = -1;
  }
}

static const mini_gnb_c_ngap_transport_ops_t k_mini_gnb_c_fake_transport_ops = {
    mini_gnb_c_fake_transport_connect,
    mini_gnb_c_fake_transport_send,
    mini_gnb_c_fake_transport_recv,
    mini_gnb_c_fake_transport_close,
};

static void mini_gnb_c_prepare_fake_amf_dialog(mini_gnb_c_fake_ngap_transport_t* fake) {
  static const uint8_t k_ngsetup_response[] = {0x20u, 0x15u, 0x00u, 0x00u};
  static const uint8_t k_auth_request_nas[] = {0x7eu, 0x00u, 0x56u, 0x01u, 0x02u, 0x03u};

  uint8_t message[MINI_GNB_C_CORE_BRIDGE_MAX_MESSAGE];
  size_t message_length = 0u;

  mini_gnb_c_require(fake != NULL, "expected fake transport for canned AMF dialog");
  memset(fake, 0, sizeof(*fake));

  memcpy(fake->responses[0], k_ngsetup_response, sizeof(k_ngsetup_response));
  fake->response_lengths[0] = sizeof(k_ngsetup_response);
  mini_gnb_c_require(mini_gnb_c_ngap_build_downlink_nas_transport(0x1234u,
                                                                  7u,
                                                                  k_auth_request_nas,
                                                                  sizeof(k_auth_request_nas),
                                                                  message,
                                                                  sizeof(message),
                                                                  &message_length) == 0,
                     "expected canned DownlinkNASTransport");
  memcpy(fake->responses[1], message, message_length);
  fake->response_lengths[1] = message_length;
  fake->response_count = 2u;
}

static void mini_gnb_c_prepare_fake_amf_dialog_with_followup(mini_gnb_c_fake_ngap_transport_t* fake) {
  static const uint8_t k_security_mode_command_nas[] = {0x7eu, 0x00u, 0x5du, 0x11u, 0x22u, 0x33u};
  uint8_t message[MINI_GNB_C_CORE_BRIDGE_MAX_MESSAGE];
  size_t message_length = 0u;

  mini_gnb_c_prepare_fake_amf_dialog(fake);
  mini_gnb_c_require(mini_gnb_c_ngap_build_downlink_nas_transport(0x1234u,
                                                                  7u,
                                                                  k_security_mode_command_nas,
                                                                  sizeof(k_security_mode_command_nas),
                                                                  message,
                                                                  sizeof(message),
                                                                  &message_length) == 0,
                     "expected canned follow-up DownlinkNASTransport");
  memcpy(fake->responses[2], message, message_length);
  fake->response_lengths[2] = message_length;
  fake->response_count = 3u;
}

static void mini_gnb_c_prepare_fake_amf_dialog_with_session_setup(mini_gnb_c_fake_ngap_transport_t* fake) {
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

  mini_gnb_c_prepare_fake_amf_dialog_with_followup(fake);
  mini_gnb_c_require(mini_gnb_c_ngap_build_initial_context_setup_response(0x1234u,
                                                                          7u,
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

void test_gnb_core_bridge_prepares_initial_ue_message(void) {
  mini_gnb_c_core_config_t config;
  mini_gnb_c_gnb_core_bridge_t bridge;
  mini_gnb_c_ue_context_t ue_context;
  mini_gnb_c_metrics_trace_t metrics;
  mini_gnb_c_fake_ngap_transport_t fake_transport;

  memset(&config, 0, sizeof(config));
  memset(&ue_context, 0, sizeof(ue_context));

  config.enabled = true;
  config.amf_port = 38412u;
  config.timeout_ms = 2500u;
  config.ran_ue_ngap_id_base = 7u;
  config.default_pdu_session_id = 1u;
  (void)snprintf(config.amf_ip, sizeof(config.amf_ip), "%s", "127.0.0.5");

  mini_gnb_c_core_session_reset(&ue_context.core_session);
  ue_context.c_rnti = 0x4601u;
  mini_gnb_c_core_session_set_c_rnti(&ue_context.core_session, ue_context.c_rnti);

  mini_gnb_c_prepare_fake_amf_dialog(&fake_transport);
  mini_gnb_c_metrics_trace_init(&metrics, "out/test_gnb_core_bridge");
  mini_gnb_c_gnb_core_bridge_init(&bridge, &config, NULL);
  mini_gnb_c_ngap_transport_set_ops(&bridge.transport, &k_mini_gnb_c_fake_transport_ops, &fake_transport);

  mini_gnb_c_require(mini_gnb_c_gnb_core_bridge_on_ue_promoted(&bridge, &ue_context, &metrics, 6) == 0,
                     "expected core bridge to exchange first NAS with fake AMF");
  mini_gnb_c_require(ue_context.core_session.ran_ue_ngap_id_valid &&
                         ue_context.core_session.ran_ue_ngap_id == 7u,
                     "expected seeded RAN UE NGAP ID");
  mini_gnb_c_require(ue_context.core_session.pdu_session_id_valid &&
                         ue_context.core_session.pdu_session_id == 1u,
                     "expected seeded requested PDU session ID");
  mini_gnb_c_require(ue_context.core_session.amf_ue_ngap_id_valid &&
                         ue_context.core_session.amf_ue_ngap_id == 0x1234u,
                     "expected parsed AMF UE NGAP ID");
  mini_gnb_c_require(ue_context.core_session.uplink_nas_count == 1u, "expected one sent uplink NAS");
  mini_gnb_c_require(ue_context.core_session.downlink_nas_count == 1u, "expected one received downlink NAS");
  mini_gnb_c_require(bridge.ng_setup_complete, "expected NGSetup to complete");
  mini_gnb_c_require(bridge.initial_message_sent, "expected InitialUEMessage to be sent");
  mini_gnb_c_require(bridge.last_initial_ue_message_length > 0u, "expected encoded InitialUEMessage");
  mini_gnb_c_require(bridge.last_initial_ue_message[0] == 0x00u && bridge.last_initial_ue_message[1] == 0x0fu,
                     "expected InitialUEMessage NGAP header");
  mini_gnb_c_require(bridge.last_initial_ue_message_abs_slot == 6, "expected InitialUEMessage slot marker");
  mini_gnb_c_require(bridge.last_downlink_nas_length == 6u, "expected captured downlink NAS bytes");
  mini_gnb_c_require(bridge.last_downlink_nas[2] == 0x56u, "expected AuthenticationRequest NAS message");
  mini_gnb_c_require(bridge.last_downlink_nas_abs_slot == 6, "expected downlink NAS slot marker");
  mini_gnb_c_require(fake_transport.sent_count == 2u, "expected NGSetup and InitialUEMessage sends");
  mini_gnb_c_require(fake_transport.sent_messages[0][0] == 0x00u && fake_transport.sent_messages[0][1] == 0x15u,
                     "expected NGSetupRequest on first send");
  mini_gnb_c_require(fake_transport.last_timeout_ms == 2500u, "expected configured transport timeout");
  mini_gnb_c_require(strcmp(fake_transport.last_amf_ip, "127.0.0.5") == 0, "expected configured AMF IP");
  mini_gnb_c_require(fake_transport.last_amf_port == 38412u, "expected configured AMF port");
  mini_gnb_c_require(metrics.event_count >= 2u, "expected NGSetup and NAS trace events");
}

void test_gnb_core_bridge_relays_followup_uplink_nas(void) {
  char exchange_dir[MINI_GNB_C_MAX_PATH];
  char second_dl_nas_path[MINI_GNB_C_MAX_PATH];
  mini_gnb_c_core_config_t config;
  mini_gnb_c_gnb_core_bridge_t bridge;
  mini_gnb_c_ue_context_t ue_context;
  mini_gnb_c_metrics_trace_t metrics;
  mini_gnb_c_fake_ngap_transport_t fake_transport;
  char* dl_nas_event_json = NULL;

  memset(&config, 0, sizeof(config));
  memset(&ue_context, 0, sizeof(ue_context));

  config.enabled = true;
  config.amf_port = 38412u;
  config.timeout_ms = 2500u;
  config.ran_ue_ngap_id_base = 7u;
  config.default_pdu_session_id = 1u;
  (void)snprintf(config.amf_ip, sizeof(config.amf_ip), "%s", "127.0.0.5");

  mini_gnb_c_core_session_reset(&ue_context.core_session);
  ue_context.c_rnti = 0x4601u;
  mini_gnb_c_core_session_set_c_rnti(&ue_context.core_session, ue_context.c_rnti);

  mini_gnb_c_make_output_dir("test_gnb_core_bridge_followup", exchange_dir, sizeof(exchange_dir));
  mini_gnb_c_prepare_fake_amf_dialog_with_followup(&fake_transport);
  mini_gnb_c_metrics_trace_init(&metrics, "out/test_gnb_core_bridge_followup");
  mini_gnb_c_gnb_core_bridge_init(&bridge, &config, exchange_dir);
  mini_gnb_c_ngap_transport_set_ops(&bridge.transport, &k_mini_gnb_c_fake_transport_ops, &fake_transport);

  mini_gnb_c_require(mini_gnb_c_gnb_core_bridge_on_ue_promoted(&bridge, &ue_context, &metrics, 6) == 0,
                     "expected first bridge NAS exchange");
  mini_gnb_c_require(mini_gnb_c_json_link_emit_event(exchange_dir,
                                                     "ue_to_gnb_nas",
                                                     "ue",
                                                     "UL_NAS",
                                                     1u,
                                                     7,
                                                     "{\"c_rnti\":17921,\"nas_hex\":\"7E005C000D0164F099F0FF00002143658789\"}",
                                                     NULL,
                                                     0u) == 0,
                     "expected follow-up UL_NAS event emission");
  mini_gnb_c_require(mini_gnb_c_gnb_core_bridge_poll_ue_nas(&bridge, &ue_context, 1u, &metrics, 7) == 0,
                     "expected bridge to relay follow-up UL_NAS");
  mini_gnb_c_require(ue_context.core_session.uplink_nas_count == 2u, "expected second uplink NAS count");
  mini_gnb_c_require(ue_context.core_session.downlink_nas_count == 2u, "expected second downlink NAS count");
  mini_gnb_c_require(bridge.next_ue_to_gnb_nas_sequence == 2u, "expected consumed UL_NAS event sequence");
  mini_gnb_c_require(bridge.next_gnb_to_ue_sequence == 3u, "expected two emitted DL_NAS events");
  mini_gnb_c_require(bridge.last_downlink_nas_length == 6u, "expected latest follow-up downlink NAS bytes");
  mini_gnb_c_require(bridge.last_downlink_nas[2] == 0x5du, "expected SecurityModeCommand NAS");
  mini_gnb_c_require(fake_transport.sent_count == 3u, "expected one extra UplinkNASTransport send");
  mini_gnb_c_require(fake_transport.sent_messages[2][0] == 0x00u && fake_transport.sent_messages[2][1] == 0x2eu,
                     "expected UplinkNASTransport on follow-up send");

  mini_gnb_c_require(mini_gnb_c_json_link_build_event_path(exchange_dir,
                                                           "gnb_to_ue",
                                                           "gnb",
                                                           2u,
                                                           "DL_NAS",
                                                           second_dl_nas_path,
                                                           sizeof(second_dl_nas_path)) == 0,
                     "expected second downlink NAS path");
  dl_nas_event_json = mini_gnb_c_read_text_file(second_dl_nas_path);
  mini_gnb_c_require(dl_nas_event_json != NULL, "expected second downlink NAS event file");
  mini_gnb_c_require(strstr(dl_nas_event_json, "\"nas_hex\":\"7E005D112233\"") != NULL,
                     "expected follow-up DL_NAS payload");
  free(dl_nas_event_json);
}

void test_gnb_core_bridge_ignores_disabled_config(void) {
  mini_gnb_c_core_config_t config;
  mini_gnb_c_gnb_core_bridge_t bridge;
  mini_gnb_c_ue_context_t ue_context;

  memset(&config, 0, sizeof(config));
  memset(&ue_context, 0, sizeof(ue_context));

  ue_context.c_rnti = 0x4601u;
  mini_gnb_c_core_session_reset(&ue_context.core_session);
  mini_gnb_c_core_session_set_c_rnti(&ue_context.core_session, ue_context.c_rnti);

  mini_gnb_c_gnb_core_bridge_init(&bridge, &config, NULL);
  mini_gnb_c_require(mini_gnb_c_gnb_core_bridge_on_ue_promoted(&bridge, &ue_context, NULL, 6) == 0,
                     "expected disabled core bridge to no-op");
  mini_gnb_c_require(!ue_context.core_session.ran_ue_ngap_id_valid, "expected no NGAP ID when disabled");
  mini_gnb_c_require(bridge.last_initial_ue_message_length == 0u, "expected no InitialUEMessage when disabled");
  mini_gnb_c_require(bridge.last_downlink_nas_length == 0u, "expected no downlink NAS when disabled");
}

void test_gnb_core_bridge_parses_session_setup_state(void) {
  char exchange_dir[MINI_GNB_C_MAX_PATH];
  mini_gnb_c_core_config_t config;
  mini_gnb_c_gnb_core_bridge_t bridge;
  mini_gnb_c_ue_context_t ue_context;
  mini_gnb_c_metrics_trace_t metrics;
  mini_gnb_c_fake_ngap_transport_t fake_transport;
  char ue_ipv4_text[MINI_GNB_C_CORE_MAX_IPV4_TEXT];

  memset(&config, 0, sizeof(config));
  memset(&ue_context, 0, sizeof(ue_context));

  config.enabled = true;
  config.amf_port = 38412u;
  config.timeout_ms = 2500u;
  config.ran_ue_ngap_id_base = 7u;
  config.default_pdu_session_id = 1u;
  (void)snprintf(config.amf_ip, sizeof(config.amf_ip), "%s", "127.0.0.5");

  mini_gnb_c_core_session_reset(&ue_context.core_session);
  ue_context.c_rnti = 0x4601u;
  mini_gnb_c_core_session_set_c_rnti(&ue_context.core_session, ue_context.c_rnti);

  mini_gnb_c_make_output_dir("test_gnb_core_bridge_session_setup", exchange_dir, sizeof(exchange_dir));
  mini_gnb_c_prepare_fake_amf_dialog_with_session_setup(&fake_transport);
  mini_gnb_c_metrics_trace_init(&metrics, "out/test_gnb_core_bridge_session_setup");
  mini_gnb_c_gnb_core_bridge_init(&bridge, &config, exchange_dir);
  mini_gnb_c_ngap_transport_set_ops(&bridge.transport, &k_mini_gnb_c_fake_transport_ops, &fake_transport);

  mini_gnb_c_require(mini_gnb_c_gnb_core_bridge_on_ue_promoted(&bridge, &ue_context, &metrics, 6) == 0,
                     "expected initial bridge exchange");
  mini_gnb_c_require(mini_gnb_c_json_link_emit_event(exchange_dir,
                                                     "ue_to_gnb_nas",
                                                     "ue",
                                                     "UL_NAS",
                                                     1u,
                                                     7,
                                                     "{\"c_rnti\":17921,\"nas_hex\":\"7E005C000D0164F099F0FF00002143658789\"}",
                                                     NULL,
                                                     0u) == 0,
                     "expected first follow-up UL_NAS emission");
  mini_gnb_c_require(mini_gnb_c_json_link_emit_event(exchange_dir,
                                                     "ue_to_gnb_nas",
                                                     "ue",
                                                     "UL_NAS",
                                                     2u,
                                                     8,
                                                     "{\"c_rnti\":17921,\"nas_hex\":\"7E005E7700098526610956163978F871002E\"}",
                                                     NULL,
                                                     0u) == 0,
                     "expected second follow-up UL_NAS emission");
  mini_gnb_c_require(mini_gnb_c_json_link_emit_event(exchange_dir,
                                                     "ue_to_gnb_nas",
                                                     "ue",
                                                     "UL_NAS",
                                                     3u,
                                                     9,
                                                     "{\"c_rnti\":17921,\"nas_hex\":\"7E004301\"}",
                                                     NULL,
                                                     0u) == 0,
                     "expected third follow-up UL_NAS emission");
  mini_gnb_c_require(mini_gnb_c_gnb_core_bridge_poll_ue_nas(&bridge, &ue_context, 1u, &metrics, 7) == 0,
                     "expected first follow-up relay");
  mini_gnb_c_require(mini_gnb_c_gnb_core_bridge_poll_ue_nas(&bridge, &ue_context, 1u, &metrics, 8) == 0,
                     "expected second follow-up relay");
  mini_gnb_c_require(mini_gnb_c_gnb_core_bridge_poll_ue_nas(&bridge, &ue_context, 1u, &metrics, 9) == 0,
                     "expected third follow-up relay");

  mini_gnb_c_require(ue_context.core_session.uplink_nas_count == 4u, "expected four uplink NAS sends");
  mini_gnb_c_require(ue_context.core_session.downlink_nas_count == 2u, "expected two downlink NAS PDUs");
  mini_gnb_c_require(strcmp(ue_context.core_session.upf_ip, "127.0.0.7") == 0,
                     "expected parsed UPF IP after session setup");
  mini_gnb_c_require(ue_context.core_session.upf_teid == 0x0000ef26u,
                     "expected parsed UPF TEID after session setup");
  mini_gnb_c_require(ue_context.core_session.qfi_valid && ue_context.core_session.qfi == 1u,
                     "expected parsed QFI after session setup");
  mini_gnb_c_require(mini_gnb_c_core_session_format_ue_ipv4(&ue_context.core_session,
                                                            ue_ipv4_text,
                                                            sizeof(ue_ipv4_text)) == 0,
                     "expected parsed UE IPv4 after session setup");
  mini_gnb_c_require(strcmp(ue_ipv4_text, "10.45.0.7") == 0, "expected parsed UE IPv4 text");
  mini_gnb_c_require(fake_transport.sent_count == 7u, "expected UL NAS sends plus NGAP acknowledgements");
  mini_gnb_c_require(fake_transport.sent_messages[4][0] == 0x20u && fake_transport.sent_messages[4][1] == 0x0eu,
                     "expected InitialContextSetupResponse send");
  mini_gnb_c_require(fake_transport.sent_messages[6][0] == 0x20u && fake_transport.sent_messages[6][1] == 0x1du,
                     "expected PDUSessionResourceSetupResponse send");
}
