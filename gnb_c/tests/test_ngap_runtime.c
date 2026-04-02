#include <string.h>

#include "test_helpers.h"

#include "mini_gnb_c/ngap/ngap_runtime.h"

void test_ngap_runtime_builders_encode_expected_headers(void) {
  static const uint8_t nas_pdu[] = {0x7eu, 0x00u, 0x5cu, 0x00u};
  uint8_t message[512];
  uint8_t extracted_nas[64];
  size_t extracted_nas_length = 0u;
  size_t message_length = 0u;
  uint16_t amf_ue_ngap_id = 0u;

  mini_gnb_c_require(mini_gnb_c_ngap_build_ng_setup_request(message, sizeof(message), &message_length) == 0,
                     "expected NGSetupRequest build");
  mini_gnb_c_require(message_length > 0u && message[0] == 0x00u && message[1] == 0x15u,
                     "expected NGSetupRequest NGAP header");

  mini_gnb_c_require(mini_gnb_c_ngap_build_initial_ue_message(nas_pdu,
                                                              sizeof(nas_pdu),
                                                              0x4601u,
                                                              message,
                                                              sizeof(message),
                                                              &message_length) == 0,
                     "expected InitialUEMessage build");
  mini_gnb_c_require(message_length > 0u && message[0] == 0x00u && message[1] == 0x0fu,
                     "expected InitialUEMessage NGAP header");

  mini_gnb_c_require(mini_gnb_c_ngap_build_uplink_nas_transport(0x1234u,
                                                                0x4601u,
                                                                nas_pdu,
                                                                sizeof(nas_pdu),
                                                                message,
                                                                sizeof(message),
                                                                &message_length) == 0,
                     "expected UplinkNASTransport build");
  mini_gnb_c_require(message_length > 0u && message[0] == 0x00u && message[1] == 0x2eu,
                     "expected UplinkNASTransport NGAP header");
  mini_gnb_c_require(mini_gnb_c_ngap_extract_amf_ue_ngap_id(message, message_length, &amf_ue_ngap_id) == 0,
                     "expected AMF UE NGAP ID extraction");
  mini_gnb_c_require(amf_ue_ngap_id == 0x1234u, "expected round-tripped AMF UE NGAP ID");

  mini_gnb_c_require(mini_gnb_c_ngap_build_downlink_nas_transport(0x1234u,
                                                                  0x4601u,
                                                                  nas_pdu,
                                                                  sizeof(nas_pdu),
                                                                  message,
                                                                  sizeof(message),
                                                                  &message_length) == 0,
                     "expected DownlinkNASTransport build");
  mini_gnb_c_require(message_length > 0u && message[0] == 0x00u && message[1] == 0x04u,
                     "expected DownlinkNASTransport NGAP header");
  mini_gnb_c_require(mini_gnb_c_ngap_extract_nas_pdu(message,
                                                     message_length,
                                                     extracted_nas,
                                                     sizeof(extracted_nas),
                                                     &extracted_nas_length) == 0,
                     "expected NAS PDU extraction");
  mini_gnb_c_require(extracted_nas_length == sizeof(nas_pdu) &&
                         memcmp(extracted_nas, nas_pdu, sizeof(nas_pdu)) == 0,
                     "expected round-tripped NAS PDU");

  mini_gnb_c_require(mini_gnb_c_ngap_build_initial_context_setup_response(0x1234u,
                                                                          0x4601u,
                                                                          message,
                                                                          sizeof(message),
                                                                          &message_length) == 0,
                     "expected InitialContextSetupResponse build");
  mini_gnb_c_require(message_length > 0u && message[0] == 0x20u && message[1] == 0x0eu,
                     "expected InitialContextSetupResponse header");

  mini_gnb_c_require(mini_gnb_c_ngap_build_pdu_session_resource_setup_response(0x1234u,
                                                                               0x4601u,
                                                                               message,
                                                                               sizeof(message),
                                                                               &message_length) == 0,
                     "expected PDUSessionResourceSetupResponse build");
  mini_gnb_c_require(message_length > 0u && message[0] == 0x20u && message[1] == 0x1du,
                     "expected PDUSessionResourceSetupResponse header");
}

void test_ngap_runtime_extracts_open5gs_user_plane_state(void) {
  static const uint8_t session_list_value[] = {
      0x7eu, 0x00u, 0x68u, 0x01u,
      0x29u, 0x05u, 0x01u, 10u, 45u, 0u, 7u,
      0x00u, 0x00u, 0x02u,
      0x00u, 0x8bu, 0x00u, 0x0au, 0x00u, 0xf0u, 127u, 0u, 0u, 7u, 0x00u, 0x00u, 0xefu, 0x26u,
      0x00u, 0x88u, 0x00u, 0x02u, 0x00u, 0x01u,
  };
  static const uint8_t ngap_message[] = {
      0x20u, 0x1du, 0x00u, 0x29u,
      0x00u, 0x00u, 0x01u,
      0x00u, 0x4au, 0x00u, 0x22u,
      0x7eu, 0x00u, 0x68u, 0x01u,
      0x29u, 0x05u, 0x01u, 10u, 45u, 0u, 7u,
      0x00u, 0x00u, 0x02u,
      0x00u, 0x8bu, 0x00u, 0x0au, 0x00u, 0xf0u, 127u, 0u, 0u, 7u, 0x00u, 0x00u, 0xefu, 0x26u,
      0x00u, 0x88u, 0x00u, 0x02u, 0x00u, 0x01u,
  };
  mini_gnb_c_core_session_t session;
  char ue_ipv4_text[MINI_GNB_C_CORE_MAX_IPV4_TEXT];

  mini_gnb_c_require(sizeof(ngap_message) == 45u, "expected canned NGAP message length");
  mini_gnb_c_require(sizeof(session_list_value) == 34u, "expected canned session list length");

  mini_gnb_c_core_session_reset(&session);
  mini_gnb_c_require(mini_gnb_c_ngap_extract_open5gs_user_plane_state(ngap_message,
                                                                      sizeof(ngap_message),
                                                                      &session) == 0,
                     "expected Open5GS user-plane state extraction");
  mini_gnb_c_require(strcmp(session.upf_ip, "127.0.0.7") == 0, "expected UPF IP extraction");
  mini_gnb_c_require(session.upf_teid == 0x0000ef26u, "expected UPF TEID extraction");
  mini_gnb_c_require(session.qfi_valid && session.qfi == 1u, "expected QFI extraction");
  mini_gnb_c_require(mini_gnb_c_core_session_format_ue_ipv4(&session, ue_ipv4_text, sizeof(ue_ipv4_text)) == 0,
                     "expected UE IPv4 formatting after extraction");
  mini_gnb_c_require(strcmp(ue_ipv4_text, "10.45.0.7") == 0, "expected UE IPv4 extraction");
}
