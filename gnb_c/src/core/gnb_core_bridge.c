#include "mini_gnb_c/core/gnb_core_bridge.h"

#include <stdio.h>
#include <string.h>

#include "mini_gnb_c/common/hex.h"
#include "mini_gnb_c/link/json_link.h"
#include "mini_gnb_c/ngap/ngap_runtime.h"

static const uint8_t k_mini_gnb_c_initial_registration_request_nas[] = {
    0x7e, 0x01, 0xda, 0xb8, 0x93, 0xa0, 0x04, 0x7e, 0x00, 0x41, 0x29, 0x00, 0x0b, 0xf2,
    0x64, 0xf0, 0x99, 0x02, 0x00, 0x40, 0xc0, 0x00, 0x06, 0x01, 0x2e, 0x02, 0xf0, 0xf0,
    0x71, 0x00, 0x2e, 0x7e, 0x00, 0x41, 0x29, 0x00, 0x0b, 0xf2, 0x64, 0xf0, 0x99, 0x02,
    0x00, 0x40, 0xc0, 0x00, 0x06, 0x01, 0x10, 0x01, 0x07, 0x2e, 0x02, 0xf0, 0xf0, 0x52,
    0x64, 0xf0, 0x99, 0x00, 0x00, 0x01, 0x17, 0x07, 0xf0, 0xf0, 0xc0, 0xc0, 0x1d, 0x80,
    0x30, 0x18, 0x01, 0x00, 0x53, 0x01, 0x01,
};

static int mini_gnb_c_gnb_core_bridge_is_expected_ngap(const uint8_t* message,
                                                       size_t message_length,
                                                       uint8_t expected_pdu_type,
                                                       uint8_t expected_procedure_code) {
  if (message == NULL || message_length < 2u) {
    return 0;
  }
  return message[0] == expected_pdu_type && message[1] == expected_procedure_code;
}

static int mini_gnb_c_gnb_core_bridge_emit_downlink_nas(mini_gnb_c_gnb_core_bridge_t* bridge,
                                                        const mini_gnb_c_ue_context_t* ue_context,
                                                        int abs_slot) {
  char nas_hex[MINI_GNB_C_MAX_PAYLOAD * 2u + 1u];
  char payload_json[MINI_GNB_C_MAX_PAYLOAD * 2u + 256u];

  if (bridge == NULL || ue_context == NULL) {
    return -1;
  }
  if (bridge->local_exchange_dir[0] == '\0' || bridge->last_downlink_nas_length == 0u) {
    return 0;
  }
  if (mini_gnb_c_bytes_to_hex(bridge->last_downlink_nas,
                              bridge->last_downlink_nas_length,
                              nas_hex,
                              sizeof(nas_hex)) != 0) {
    return -1;
  }
  if (snprintf(payload_json,
               sizeof(payload_json),
               "{\"c_rnti\":%u,\"ran_ue_ngap_id\":%u,\"amf_ue_ngap_id\":%u,\"nas_hex\":\"%s\"}",
               (unsigned)ue_context->c_rnti,
               (unsigned)(ue_context->core_session.ran_ue_ngap_id_valid ? ue_context->core_session.ran_ue_ngap_id
                                                                        : 0u),
               (unsigned)(ue_context->core_session.amf_ue_ngap_id_valid ? ue_context->core_session.amf_ue_ngap_id
                                                                        : 0u),
               nas_hex) >= (int)sizeof(payload_json)) {
    return -1;
  }

  return mini_gnb_c_json_link_emit_event(bridge->local_exchange_dir,
                                         "gnb_to_ue",
                                         "gnb",
                                         "DL_NAS",
                                         bridge->next_gnb_to_ue_sequence++,
                                         abs_slot,
                                         payload_json,
                                         NULL,
                                         0u);
}

static int mini_gnb_c_gnb_core_bridge_run_ng_setup(mini_gnb_c_gnb_core_bridge_t* bridge,
                                                   mini_gnb_c_metrics_trace_t* metrics,
                                                   int abs_slot) {
  uint8_t request[MINI_GNB_C_CORE_BRIDGE_MAX_MESSAGE];
  uint8_t response[MINI_GNB_C_CORE_BRIDGE_MAX_MESSAGE];
  size_t request_length = 0u;
  size_t response_length = 0u;

  if (bridge == NULL) {
    return -1;
  }
  if (bridge->ng_setup_complete) {
    return 0;
  }
  if (mini_gnb_c_ngap_transport_connect(&bridge->transport,
                                        bridge->config.amf_ip,
                                        bridge->config.amf_port,
                                        bridge->config.timeout_ms) != 0) {
    return -1;
  }
  if (mini_gnb_c_ngap_build_ng_setup_request(request, sizeof(request), &request_length) != 0) {
    return -1;
  }
  if (mini_gnb_c_ngap_transport_send(&bridge->transport, request, request_length) != 0 ||
      mini_gnb_c_ngap_transport_recv(&bridge->transport, response, sizeof(response), &response_length) != 0) {
    return -1;
  }
  if (!mini_gnb_c_gnb_core_bridge_is_expected_ngap(response, response_length, 0x20u, 0x15u)) {
    return -1;
  }

  bridge->ng_setup_complete = true;
  if (metrics != NULL) {
    mini_gnb_c_metrics_trace_event(metrics,
                                   "gnb_core_bridge",
                                   "Completed NGSetup with AMF over SCTP.",
                                   abs_slot,
                                   "amf=%s:%u,request_length=%zu,response_length=%zu",
                                   bridge->config.amf_ip,
                                   bridge->config.amf_port,
                                   request_length,
                                   response_length);
  }
  return 0;
}

static int mini_gnb_c_gnb_core_bridge_send_initial_ue_message(mini_gnb_c_gnb_core_bridge_t* bridge,
                                                              mini_gnb_c_ue_context_t* ue_context,
                                                              mini_gnb_c_metrics_trace_t* metrics,
                                                              int abs_slot) {
  uint8_t response[MINI_GNB_C_CORE_BRIDGE_MAX_MESSAGE];
  uint16_t amf_ue_ngap_id = 0u;
  size_t response_length = 0u;

  if (bridge == NULL || ue_context == NULL) {
    return -1;
  }
  if (bridge->initial_message_sent) {
    return 0;
  }
  if (mini_gnb_c_ngap_build_initial_ue_message(k_mini_gnb_c_initial_registration_request_nas,
                                               sizeof(k_mini_gnb_c_initial_registration_request_nas),
                                               ue_context->core_session.ran_ue_ngap_id,
                                               bridge->last_initial_ue_message,
                                               sizeof(bridge->last_initial_ue_message),
                                               &bridge->last_initial_ue_message_length) != 0) {
    return -1;
  }
  bridge->last_initial_ue_message_abs_slot = abs_slot;

  if (mini_gnb_c_ngap_transport_send(&bridge->transport,
                                     bridge->last_initial_ue_message,
                                     bridge->last_initial_ue_message_length) != 0 ||
      mini_gnb_c_ngap_transport_recv(&bridge->transport, response, sizeof(response), &response_length) != 0) {
    return -1;
  }
  if (mini_gnb_c_ngap_extract_amf_ue_ngap_id(response, response_length, &amf_ue_ngap_id) != 0 ||
      mini_gnb_c_ngap_extract_nas_pdu(response,
                                      response_length,
                                      bridge->last_downlink_nas,
                                      sizeof(bridge->last_downlink_nas),
                                      &bridge->last_downlink_nas_length) != 0) {
    return -1;
  }

  mini_gnb_c_core_session_set_amf_ue_ngap_id(&ue_context->core_session, amf_ue_ngap_id);
  mini_gnb_c_core_session_increment_uplink_nas_count(&ue_context->core_session);
  mini_gnb_c_core_session_increment_downlink_nas_count(&ue_context->core_session);
  bridge->last_downlink_nas_abs_slot = abs_slot;
  bridge->initial_message_sent = true;

  if (mini_gnb_c_gnb_core_bridge_emit_downlink_nas(bridge, ue_context, abs_slot) != 0) {
    return -1;
  }
  if (metrics != NULL) {
    mini_gnb_c_metrics_trace_event(metrics,
                                   "gnb_core_bridge",
                                   "Forwarded InitialUEMessage to AMF and captured first DL NAS.",
                                   abs_slot,
                                   "ran_ue_ngap_id=%u,amf_ue_ngap_id=%u,initial_length=%zu,dl_nas_length=%zu",
                                   (unsigned)ue_context->core_session.ran_ue_ngap_id,
                                   (unsigned)ue_context->core_session.amf_ue_ngap_id,
                                   bridge->last_initial_ue_message_length,
                                   bridge->last_downlink_nas_length);
  }
  return 0;
}

void mini_gnb_c_gnb_core_bridge_init(mini_gnb_c_gnb_core_bridge_t* bridge,
                                     const mini_gnb_c_core_config_t* config,
                                     const char* local_exchange_dir) {
  if (bridge == NULL) {
    return;
  }

  memset(bridge, 0, sizeof(*bridge));
  mini_gnb_c_ngap_transport_init(&bridge->transport);
  if (config != NULL) {
    bridge->config = *config;
    bridge->next_ran_ue_ngap_id = config->ran_ue_ngap_id_base;
  }
  if (local_exchange_dir != NULL) {
    (void)snprintf(bridge->local_exchange_dir, sizeof(bridge->local_exchange_dir), "%s", local_exchange_dir);
  }
  bridge->next_gnb_to_ue_sequence = 1u;
  bridge->last_initial_ue_message_abs_slot = -1;
  bridge->last_downlink_nas_abs_slot = -1;
}

int mini_gnb_c_gnb_core_bridge_on_ue_promoted(mini_gnb_c_gnb_core_bridge_t* bridge,
                                              mini_gnb_c_ue_context_t* ue_context,
                                              mini_gnb_c_metrics_trace_t* metrics,
                                              int abs_slot) {
  mini_gnb_c_core_session_t* core_session = NULL;

  if (bridge == NULL || ue_context == NULL) {
    return -1;
  }
  if (!bridge->config.enabled) {
    return 0;
  }

  core_session = &ue_context->core_session;
  if (!core_session->ran_ue_ngap_id_valid) {
    mini_gnb_c_core_session_set_ran_ue_ngap_id(core_session, bridge->next_ran_ue_ngap_id);
    if (bridge->next_ran_ue_ngap_id != UINT16_MAX) {
      ++bridge->next_ran_ue_ngap_id;
    }
  }
  if (!core_session->pdu_session_id_valid && bridge->config.default_pdu_session_id > 0u) {
    (void)mini_gnb_c_core_session_set_pdu_session_id(core_session, bridge->config.default_pdu_session_id);
  }
  if (mini_gnb_c_gnb_core_bridge_run_ng_setup(bridge, metrics, abs_slot) != 0) {
    return -1;
  }
  if (mini_gnb_c_gnb_core_bridge_send_initial_ue_message(bridge, ue_context, metrics, abs_slot) != 0) {
    return -1;
  }

  return 0;
}
