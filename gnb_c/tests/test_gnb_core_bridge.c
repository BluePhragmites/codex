#include <string.h>

#include "test_helpers.h"

#include "mini_gnb_c/core/gnb_core_bridge.h"
#include "mini_gnb_c/ngap/ngap_runtime.h"

typedef struct {
  uint8_t responses[2][MINI_GNB_C_CORE_BRIDGE_MAX_MESSAGE];
  size_t response_lengths[2];
  size_t response_count;
  size_t response_index;
  uint8_t sent_messages[2][MINI_GNB_C_CORE_BRIDGE_MAX_MESSAGE];
  size_t sent_lengths[2];
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
  mini_gnb_c_require(fake->sent_count < 2u, "unexpected extra send");
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
