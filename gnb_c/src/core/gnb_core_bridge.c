#include "mini_gnb_c/core/gnb_core_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mini_gnb_c/common/hex.h"
#include "mini_gnb_c/n3/n3_user_plane.h"
#include "mini_gnb_c/common/json_utils.h"
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
static const char* k_mini_gnb_c_ue_to_gnb_nas_channel = "ue_to_gnb_nas";

static void mini_gnb_c_gnb_core_bridge_trace_ngap(mini_gnb_c_gnb_core_bridge_t* bridge,
                                                  const uint8_t* message,
                                                  size_t message_length) {
  if (bridge == NULL || message == NULL || message_length == 0u ||
      !mini_gnb_c_pcap_writer_is_open(&bridge->ngap_trace_writer)) {
    return;
  }
  (void)mini_gnb_c_pcap_writer_write(&bridge->ngap_trace_writer, message, message_length);
}

static int mini_gnb_c_gnb_core_bridge_emit_downlink_nas(mini_gnb_c_gnb_core_bridge_t* bridge,
                                                        const mini_gnb_c_ue_context_t* ue_context,
                                                        int abs_slot);
static int mini_gnb_c_gnb_core_bridge_send_ngap_response(mini_gnb_c_gnb_core_bridge_t* bridge,
                                                         mini_gnb_c_ue_context_t* ue_context,
                                                         uint8_t procedure_code,
                                                         mini_gnb_c_metrics_trace_t* metrics,
                                                         int abs_slot);

static mini_gnb_c_ue_context_t* mini_gnb_c_gnb_core_bridge_find_ue_context(mini_gnb_c_ue_context_t* ue_contexts,
                                                                            size_t ue_context_count,
                                                                            uint16_t c_rnti) {
  size_t index = 0u;

  if (ue_contexts == NULL) {
    return NULL;
  }
  for (index = 0u; index < ue_context_count; ++index) {
    if (ue_contexts[index].c_rnti == c_rnti) {
      return &ue_contexts[index];
    }
  }
  return NULL;
}

static int mini_gnb_c_gnb_core_bridge_is_expected_ngap(const uint8_t* message,
                                                       size_t message_length,
                                                       uint8_t expected_pdu_type,
                                                       uint8_t expected_procedure_code) {
  if (message == NULL || message_length < 2u) {
    return 0;
  }
  return message[0] == expected_pdu_type && message[1] == expected_procedure_code;
}

static int mini_gnb_c_gnb_core_bridge_apply_amf_response(mini_gnb_c_gnb_core_bridge_t* bridge,
                                                         mini_gnb_c_ue_context_t* ue_context,
                                                         const uint8_t* response,
                                                         size_t response_length,
                                                         mini_gnb_c_metrics_trace_t* metrics,
                                                         int abs_slot,
                                                         const char* context_label) {
  uint16_t amf_ue_ngap_id = 0u;
  size_t downlink_nas_length = 0u;
  const uint8_t pdu_type = response_length > 0u ? response[0] : 0xffu;
  const uint8_t procedure_code = response_length > 1u ? response[1] : 0xffu;

  if (bridge == NULL || ue_context == NULL || response == NULL || response_length == 0u) {
    return -1;
  }
  if (mini_gnb_c_ngap_extract_amf_ue_ngap_id(response, response_length, &amf_ue_ngap_id) == 0) {
    mini_gnb_c_core_session_set_amf_ue_ngap_id(&ue_context->core_session, amf_ue_ngap_id);
  }
  if (mini_gnb_c_ngap_extract_nas_pdu(response,
                                      response_length,
                                      bridge->last_downlink_nas,
                                      sizeof(bridge->last_downlink_nas),
                                      &downlink_nas_length) == 0) {
    bridge->last_downlink_nas_length = downlink_nas_length;
    bridge->last_downlink_nas_abs_slot = abs_slot;
    mini_gnb_c_core_session_increment_downlink_nas_count(&ue_context->core_session);
    if (mini_gnb_c_gnb_core_bridge_emit_downlink_nas(bridge, ue_context, abs_slot) != 0) {
      return -1;
    }
    if (metrics != NULL) {
      mini_gnb_c_metrics_trace_event(metrics,
                                     "gnb_core_bridge",
                                     "Forwarded AMF downlink NAS into the local UE exchange.",
                                     abs_slot,
                                     "context=%s,amf_ue_ngap_id=%u,dl_nas_length=%zu,pdu_type=0x%02x,proc=0x%02x",
                                     context_label != NULL ? context_label : "(unknown)",
                                     (unsigned)ue_context->core_session.amf_ue_ngap_id,
                                     downlink_nas_length,
                                     response[0],
                                     response[1]);
    }
  }

  if (pdu_type == 0x00u && procedure_code == 0x1du) {
    if (mini_gnb_c_ngap_extract_open5gs_user_plane_state(response, response_length, &ue_context->core_session) != 0) {
      return -1;
    }
    if (metrics != NULL) {
      char ue_ipv4_text[MINI_GNB_C_CORE_MAX_IPV4_TEXT];

      if (mini_gnb_c_core_session_format_ue_ipv4(&ue_context->core_session, ue_ipv4_text, sizeof(ue_ipv4_text)) !=
          0) {
        (void)snprintf(ue_ipv4_text, sizeof(ue_ipv4_text), "%s", "(unavailable)");
      }
      mini_gnb_c_metrics_trace_event(metrics,
                                     "gnb_core_bridge",
                                     "Parsed Open5GS user-plane state from PDUSessionResourceSetupRequest.",
                                     abs_slot,
                                     "context=%s,ue_ipv4=%s,upf=%s,teid=0x%08x,qfi=%u",
                                     context_label != NULL ? context_label : "(unknown)",
                                     ue_ipv4_text,
                                     ue_context->core_session.upf_ip,
                                     ue_context->core_session.upf_teid,
                                     (unsigned)ue_context->core_session.qfi);
    }
  }
  if (pdu_type == 0x00u && (procedure_code == 0x0eu || procedure_code == 0x1du) &&
      mini_gnb_c_gnb_core_bridge_send_ngap_response(bridge, ue_context, procedure_code, metrics, abs_slot) != 0) {
    return -1;
  }

  return 0;
}

static int mini_gnb_c_gnb_core_bridge_emit_downlink_nas(mini_gnb_c_gnb_core_bridge_t* bridge,
                                                        const mini_gnb_c_ue_context_t* ue_context,
                                                        int abs_slot) {
  char nas_hex[MINI_GNB_C_MAX_PAYLOAD * 2u + 1u];
  char payload_json[MINI_GNB_C_MAX_PAYLOAD * 2u + 256u];

  if (bridge == NULL || ue_context == NULL) {
    return -1;
  }
  if (bridge->radio_nas_transport_enabled) {
    if (bridge->last_downlink_nas_length == 0u ||
        mini_gnb_c_buffer_set_bytes(&bridge->pending_downlink_nas,
                                    bridge->last_downlink_nas,
                                    bridge->last_downlink_nas_length) != 0) {
      return -1;
    }
    bridge->pending_downlink_c_rnti = ue_context->c_rnti;
    bridge->pending_downlink_nas_valid = true;
    return 0;
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

static int mini_gnb_c_gnb_core_bridge_send_uplink_nas(mini_gnb_c_gnb_core_bridge_t* bridge,
                                                      mini_gnb_c_ue_context_t* ue_context,
                                                      const uint8_t* nas_pdu,
                                                      size_t nas_pdu_length,
                                                      mini_gnb_c_metrics_trace_t* metrics,
                                                      int abs_slot,
                                                      const char* context_label) {
  uint8_t request[MINI_GNB_C_CORE_BRIDGE_MAX_MESSAGE];
  uint8_t response[MINI_GNB_C_CORE_BRIDGE_MAX_MESSAGE];
  size_t request_length = 0u;
  size_t response_length = 0u;

  if (bridge == NULL || ue_context == NULL || nas_pdu == NULL || nas_pdu_length == 0u ||
      !ue_context->core_session.ran_ue_ngap_id_valid || !ue_context->core_session.amf_ue_ngap_id_valid) {
    return -1;
  }
  if (mini_gnb_c_ngap_build_uplink_nas_transport(ue_context->core_session.amf_ue_ngap_id,
                                                 ue_context->core_session.ran_ue_ngap_id,
                                                 nas_pdu,
                                                 nas_pdu_length,
                                                 request,
                                                 sizeof(request),
                                                 &request_length) != 0) {
    return -1;
  }
  mini_gnb_c_gnb_core_bridge_trace_ngap(bridge, request, request_length);
  if (mini_gnb_c_ngap_transport_send(&bridge->transport, request, request_length) != 0 ||
      mini_gnb_c_ngap_transport_recv(&bridge->transport, response, sizeof(response), &response_length) != 0) {
    return -1;
  }
  mini_gnb_c_gnb_core_bridge_trace_ngap(bridge, response, response_length);

  mini_gnb_c_core_session_increment_uplink_nas_count(&ue_context->core_session);
  if (mini_gnb_c_gnb_core_bridge_apply_amf_response(bridge,
                                                    ue_context,
                                                    response,
                                                    response_length,
                                                    metrics,
                                                    abs_slot,
                                                    context_label) != 0) {
    return -1;
  }
  if (metrics != NULL) {
    mini_gnb_c_metrics_trace_event(metrics,
                                   "gnb_core_bridge",
                                   "Forwarded UE uplink NAS to the AMF.",
                                   abs_slot,
                                   "context=%s,c_rnti=%u,ran_ue_ngap_id=%u,amf_ue_ngap_id=%u,ul_nas_length=%zu,"
                                   "response_pdu=0x%02x,response_proc=0x%02x",
                                   context_label != NULL ? context_label : "(unknown)",
                                   (unsigned)ue_context->c_rnti,
                                   (unsigned)ue_context->core_session.ran_ue_ngap_id,
                                   (unsigned)ue_context->core_session.amf_ue_ngap_id,
                                   nas_pdu_length,
                                   response[0],
                                   response[1]);
  }

  return 0;
}

static int mini_gnb_c_gnb_core_bridge_send_ngap_response(mini_gnb_c_gnb_core_bridge_t* bridge,
                                                         mini_gnb_c_ue_context_t* ue_context,
                                                         uint8_t procedure_code,
                                                         mini_gnb_c_metrics_trace_t* metrics,
                                                         int abs_slot) {
  uint8_t response[MINI_GNB_C_CORE_BRIDGE_MAX_MESSAGE];
  size_t response_length = 0u;
  const char* label = NULL;
  char local_n3_ip[MINI_GNB_C_CORE_MAX_IPV4_TEXT];
  const uint16_t resolved_upf_port =
      bridge != NULL && bridge->config.upf_port != 0u ? bridge->config.upf_port : MINI_GNB_C_N3_GTPU_PORT;

  if (bridge == NULL || ue_context == NULL || !ue_context->core_session.amf_ue_ngap_id_valid ||
      !ue_context->core_session.ran_ue_ngap_id_valid) {
    return -1;
  }

  if (procedure_code == 0x0eu) {
    if (mini_gnb_c_ngap_build_initial_context_setup_response(ue_context->core_session.amf_ue_ngap_id,
                                                             ue_context->core_session.ran_ue_ngap_id,
                                                             response,
                                                             sizeof(response),
                                                             &response_length) != 0) {
      return -1;
    }
    label = "InitialContextSetupResponse";
  } else if (procedure_code == 0x1du) {
    if (mini_gnb_c_n3_user_plane_resolve_local_ipv4(ue_context->core_session.upf_ip,
                                                    resolved_upf_port,
                                                    local_n3_ip,
                                                    sizeof(local_n3_ip)) != 0 ||
        mini_gnb_c_ngap_build_pdu_session_resource_setup_response_with_tunnel(
            ue_context->core_session.amf_ue_ngap_id,
            ue_context->core_session.ran_ue_ngap_id,
            local_n3_ip,
            MINI_GNB_C_N3_DOWNLINK_TEID,
            response,
            sizeof(response),
            &response_length) != 0) {
      return -1;
    }
    label = "PDUSessionResourceSetupResponse";
  } else {
    return 0;
  }

  if (mini_gnb_c_ngap_transport_send(&bridge->transport, response, response_length) != 0) {
    return -1;
  }
  mini_gnb_c_gnb_core_bridge_trace_ngap(bridge, response, response_length);
  if (metrics != NULL) {
    mini_gnb_c_metrics_trace_event(metrics,
                                   "gnb_core_bridge",
                                   "Sent follow-up NGAP response back to the AMF.",
                                   abs_slot,
                                   "label=%s,c_rnti=%u,ran_ue_ngap_id=%u,amf_ue_ngap_id=%u,length=%zu",
                                   label,
                                   (unsigned)ue_context->c_rnti,
                                   (unsigned)ue_context->core_session.ran_ue_ngap_id,
                                   (unsigned)ue_context->core_session.amf_ue_ngap_id,
                                   response_length);
  }

  return 0;
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
  mini_gnb_c_gnb_core_bridge_trace_ngap(bridge, request, request_length);
  mini_gnb_c_gnb_core_bridge_trace_ngap(bridge, response, response_length);
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
  if (bridge == NULL || ue_context == NULL) {
    return -1;
  }
  if (bridge->initial_message_sent) {
    return 0;
  }
  return mini_gnb_c_gnb_core_bridge_submit_uplink_nas(bridge,
                                                      ue_context,
                                                      k_mini_gnb_c_initial_registration_request_nas,
                                                      sizeof(k_mini_gnb_c_initial_registration_request_nas),
                                                      metrics,
                                                      abs_slot);
}

void mini_gnb_c_gnb_core_bridge_init(mini_gnb_c_gnb_core_bridge_t* bridge,
                                     const mini_gnb_c_core_config_t* config,
                                     const char* local_exchange_dir) {
  if (bridge == NULL) {
    return;
  }

  memset(bridge, 0, sizeof(*bridge));
  mini_gnb_c_ngap_transport_init(&bridge->transport);
  mini_gnb_c_pcap_writer_init(&bridge->ngap_trace_writer);
  if (config != NULL) {
    bridge->config = *config;
    bridge->next_ran_ue_ngap_id = config->ran_ue_ngap_id_base;
  }
  if (local_exchange_dir != NULL) {
    (void)snprintf(bridge->local_exchange_dir, sizeof(bridge->local_exchange_dir), "%s", local_exchange_dir);
  }
  bridge->next_gnb_to_ue_sequence = 1u;
  bridge->next_ue_to_gnb_nas_sequence = 1u;
  bridge->last_initial_ue_message_abs_slot = -1;
  bridge->last_downlink_nas_abs_slot = -1;
}

void mini_gnb_c_gnb_core_bridge_set_radio_nas_transport(mini_gnb_c_gnb_core_bridge_t* bridge, const bool enabled) {
  if (bridge == NULL) {
    return;
  }
  bridge->radio_nas_transport_enabled = enabled;
}

int mini_gnb_c_gnb_core_bridge_set_ngap_trace_path(mini_gnb_c_gnb_core_bridge_t* bridge, const char* path) {
  if (bridge == NULL) {
    return -1;
  }
  if (path == NULL || path[0] == '\0') {
    mini_gnb_c_pcap_writer_close(&bridge->ngap_trace_writer);
    bridge->ngap_trace_writer.path[0] = '\0';
    return 0;
  }
  return mini_gnb_c_pcap_writer_open(&bridge->ngap_trace_writer, path, MINI_GNB_C_PCAP_LINKTYPE_USER5);
}

const char* mini_gnb_c_gnb_core_bridge_get_ngap_trace_path(const mini_gnb_c_gnb_core_bridge_t* bridge) {
  return bridge != NULL ? mini_gnb_c_pcap_writer_path(&bridge->ngap_trace_writer) : "";
}

void mini_gnb_c_gnb_core_bridge_close(mini_gnb_c_gnb_core_bridge_t* bridge) {
  if (bridge == NULL) {
    return;
  }
  mini_gnb_c_pcap_writer_close(&bridge->ngap_trace_writer);
  mini_gnb_c_ngap_transport_close(&bridge->transport);
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

int mini_gnb_c_gnb_core_bridge_submit_uplink_nas(mini_gnb_c_gnb_core_bridge_t* bridge,
                                                 mini_gnb_c_ue_context_t* ue_context,
                                                 const uint8_t* nas_pdu,
                                                 const size_t nas_pdu_length,
                                                 mini_gnb_c_metrics_trace_t* metrics,
                                                 const int abs_slot) {
  uint8_t response[MINI_GNB_C_CORE_BRIDGE_MAX_MESSAGE];
  size_t response_length = 0u;

  if (bridge == NULL || ue_context == NULL || nas_pdu == NULL || nas_pdu_length == 0u) {
    return -1;
  }
  if (!bridge->config.enabled) {
    return 0;
  }
  if (!ue_context->core_session.ran_ue_ngap_id_valid) {
    mini_gnb_c_core_session_set_ran_ue_ngap_id(&ue_context->core_session, bridge->next_ran_ue_ngap_id);
    if (bridge->next_ran_ue_ngap_id != UINT16_MAX) {
      ++bridge->next_ran_ue_ngap_id;
    }
  }
  if (!ue_context->core_session.pdu_session_id_valid && bridge->config.default_pdu_session_id > 0u) {
    (void)mini_gnb_c_core_session_set_pdu_session_id(&ue_context->core_session, bridge->config.default_pdu_session_id);
  }
  if (mini_gnb_c_gnb_core_bridge_run_ng_setup(bridge, metrics, abs_slot) != 0) {
    return -1;
  }

  if (!bridge->initial_message_sent) {
    if (mini_gnb_c_ngap_build_initial_ue_message(nas_pdu,
                                                 nas_pdu_length,
                                                 ue_context->core_session.ran_ue_ngap_id,
                                                 bridge->last_initial_ue_message,
                                                 sizeof(bridge->last_initial_ue_message),
                                                 &bridge->last_initial_ue_message_length) != 0) {
      return -1;
    }
    bridge->last_initial_ue_message_abs_slot = abs_slot;
    mini_gnb_c_gnb_core_bridge_trace_ngap(bridge,
                                          bridge->last_initial_ue_message,
                                          bridge->last_initial_ue_message_length);
    if (mini_gnb_c_ngap_transport_send(&bridge->transport,
                                       bridge->last_initial_ue_message,
                                       bridge->last_initial_ue_message_length) != 0 ||
        mini_gnb_c_ngap_transport_recv(&bridge->transport, response, sizeof(response), &response_length) != 0) {
      return -1;
    }
    mini_gnb_c_gnb_core_bridge_trace_ngap(bridge, response, response_length);
    if (mini_gnb_c_gnb_core_bridge_apply_amf_response(bridge,
                                                      ue_context,
                                                      response,
                                                      response_length,
                                                      metrics,
                                                      abs_slot,
                                                      "InitialUEMessage") != 0 ||
        !ue_context->core_session.amf_ue_ngap_id_valid || bridge->last_downlink_nas_length == 0u) {
      return -1;
    }
    mini_gnb_c_core_session_increment_uplink_nas_count(&ue_context->core_session);
    bridge->initial_message_sent = true;
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

  return mini_gnb_c_gnb_core_bridge_send_uplink_nas(bridge,
                                                    ue_context,
                                                    nas_pdu,
                                                    nas_pdu_length,
                                                    metrics,
                                                    abs_slot,
                                                    "UplinkNASTransport");
}

int mini_gnb_c_gnb_core_bridge_poll_ue_nas(mini_gnb_c_gnb_core_bridge_t* bridge,
                                           mini_gnb_c_ue_context_t* ue_contexts,
                                           size_t ue_context_count,
                                           mini_gnb_c_metrics_trace_t* metrics,
                                           int abs_slot) {
  if (bridge == NULL) {
    return -1;
  }
  if (ue_contexts == NULL || ue_context_count == 0u) {
    return 0;
  }
  if (bridge->radio_nas_transport_enabled) {
    return 0;
  }
  if (!bridge->config.enabled || bridge->local_exchange_dir[0] == '\0' || !bridge->initial_message_sent) {
    return 0;
  }

  while (1) {
    char event_path[MINI_GNB_C_MAX_PATH];
    char nas_hex[MINI_GNB_C_MAX_PAYLOAD * 2u + 1u];
    char* event_text = NULL;
    mini_gnb_c_ue_context_t* ue_context = NULL;
    uint8_t nas_pdu[MINI_GNB_C_MAX_PAYLOAD];
    size_t nas_pdu_length = 0u;
    int event_abs_slot = 0;
    int event_c_rnti = 0;

    if (mini_gnb_c_json_link_find_event_path(bridge->local_exchange_dir,
                                             k_mini_gnb_c_ue_to_gnb_nas_channel,
                                             "ue",
                                             bridge->next_ue_to_gnb_nas_sequence,
                                             event_path,
                                             sizeof(event_path)) != 0) {
      return 0;
    }

    event_text = mini_gnb_c_read_text_file(event_path);
    if (event_text == NULL) {
      return -1;
    }
    if (mini_gnb_c_extract_json_int(event_text, "abs_slot", &event_abs_slot) != 0 ||
        mini_gnb_c_extract_json_int(event_text, "c_rnti", &event_c_rnti) != 0 ||
        mini_gnb_c_extract_json_string(event_text, "nas_hex", nas_hex, sizeof(nas_hex)) != 0 ||
        mini_gnb_c_hex_to_bytes(nas_hex, nas_pdu, sizeof(nas_pdu), &nas_pdu_length) != 0) {
      free(event_text);
      return -1;
    }
    free(event_text);

    if (event_abs_slot > abs_slot) {
      return 0;
    }
    if (event_abs_slot < abs_slot) {
      ++bridge->next_ue_to_gnb_nas_sequence;
      if (metrics != NULL) {
        mini_gnb_c_metrics_trace_event(metrics,
                                       "gnb_core_bridge",
                                       "Skipped stale UE UL_NAS event.",
                                       abs_slot,
                                       "path=%s,event_abs_slot=%d,current_abs_slot=%d,c_rnti=%u",
                                       event_path,
                                       event_abs_slot,
                                       abs_slot,
                                       (unsigned)event_c_rnti);
      }
      continue;
    }

    ue_context = mini_gnb_c_gnb_core_bridge_find_ue_context(ue_contexts, ue_context_count, (uint16_t)event_c_rnti);
    if (ue_context == NULL) {
      return -1;
    }
    if (mini_gnb_c_gnb_core_bridge_send_uplink_nas(bridge,
                                                   ue_context,
                                                   nas_pdu,
                                                   nas_pdu_length,
                                                   metrics,
                                                   abs_slot,
                                                   "UplinkNASTransport") != 0) {
      return -1;
    }
    ++bridge->next_ue_to_gnb_nas_sequence;
  }
}

int mini_gnb_c_gnb_core_bridge_take_pending_downlink_nas(mini_gnb_c_gnb_core_bridge_t* bridge,
                                                         uint16_t* out_c_rnti,
                                                         mini_gnb_c_buffer_t* out_nas_pdu) {
  if (bridge == NULL) {
    return -1;
  }
  if (!bridge->pending_downlink_nas_valid) {
    if (out_c_rnti != NULL) {
      *out_c_rnti = 0u;
    }
    if (out_nas_pdu != NULL) {
      mini_gnb_c_buffer_reset(out_nas_pdu);
    }
    return 0;
  }
  if (out_c_rnti != NULL) {
    *out_c_rnti = bridge->pending_downlink_c_rnti;
  }
  if (out_nas_pdu != NULL) {
    *out_nas_pdu = bridge->pending_downlink_nas;
  }
  bridge->pending_downlink_c_rnti = 0u;
  bridge->pending_downlink_nas_valid = false;
  mini_gnb_c_buffer_reset(&bridge->pending_downlink_nas);
  return 1;
}
