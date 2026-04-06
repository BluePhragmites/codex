#include <string.h>

#include "test_helpers.h"

#include "mini_gnb_c/core/core_session.h"
#include "mini_gnb_c/n3/gtpu_tunnel.h"

static void mini_gnb_c_fill_test_session(mini_gnb_c_core_session_t* session) {
  const uint8_t ue_ipv4[4] = {10u, 45u, 0u, 7u};

  mini_gnb_c_core_session_reset(session);
  mini_gnb_c_require(mini_gnb_c_core_session_set_upf_tunnel(session, "127.0.0.7", 0x0000ef26u) == 0,
                     "expected test UPF tunnel");
  mini_gnb_c_require(mini_gnb_c_core_session_set_qfi(session, 1u) == 0, "expected test QFI");
  mini_gnb_c_core_session_set_ue_ipv4(session, ue_ipv4);
}

void test_gtpu_builders_encode_expected_headers(void) {
  mini_gnb_c_core_session_t session;
  uint8_t echo_request[32];
  size_t echo_request_length = 0u;
  uint8_t echo_response[32];
  uint8_t inner_packet[128];
  size_t inner_packet_length = 0u;
  uint8_t gtpu_packet[256];
  size_t gtpu_packet_length = 0u;
  uint8_t extracted_inner_packet[128];
  size_t extracted_inner_packet_length = 0u;
  uint32_t extracted_teid = 0u;
  uint8_t extracted_qfi = 0u;

  mini_gnb_c_fill_test_session(&session);

  mini_gnb_c_require(mini_gnb_c_gtpu_build_echo_request(1u,
                                                        echo_request,
                                                        sizeof(echo_request),
                                                        &echo_request_length) == 0,
                     "expected GTP-U Echo Request build");
  mini_gnb_c_require(echo_request_length == 14u, "expected Echo Request length");
  mini_gnb_c_require(echo_request[0] == 0x32u && echo_request[1] == 0x01u,
                     "expected Echo Request header");
  memcpy(echo_response, echo_request, echo_request_length);
  echo_response[1] = 0x02u;
  mini_gnb_c_require(mini_gnb_c_gtpu_validate_echo_response(echo_response, echo_request_length, 1u) == 0,
                     "expected Echo Response validation");

  mini_gnb_c_require(mini_gnb_c_gtpu_build_ipv4_udp_probe(&session,
                                                          "10.45.0.1",
                                                          inner_packet,
                                                          sizeof(inner_packet),
                                                          &inner_packet_length) == 0,
                     "expected inner IPv4/UDP probe build");
  mini_gnb_c_require(inner_packet_length == 43u, "expected IPv4/UDP probe size");
  mini_gnb_c_require(inner_packet[0] == 0x45u && inner_packet[9] == 17u, "expected IPv4/UDP headers");
  mini_gnb_c_require(memcmp(inner_packet + 12u, session.ue_ipv4, 4u) == 0, "expected UE IPv4 source");
  mini_gnb_c_require(inner_packet[16] == 10u && inner_packet[17] == 45u && inner_packet[18] == 0u &&
                         inner_packet[19] == 1u,
                     "expected probe destination IPv4");

  mini_gnb_c_require(mini_gnb_c_gtpu_build_gpdu(&session,
                                                inner_packet,
                                                inner_packet_length,
                                                gtpu_packet,
                                                sizeof(gtpu_packet),
                                                &gtpu_packet_length) == 0,
                     "expected G-PDU build");
  mini_gnb_c_require(gtpu_packet_length == inner_packet_length + 16u, "expected G-PDU total length");
  mini_gnb_c_require(gtpu_packet[0] == 0x34u && gtpu_packet[1] == 0xffu, "expected G-PDU header");
  mini_gnb_c_require(gtpu_packet[4] == 0x00u && gtpu_packet[5] == 0x00u && gtpu_packet[6] == 0xefu &&
                         gtpu_packet[7] == 0x26u,
                     "expected TEID bytes");
  mini_gnb_c_require(gtpu_packet[11] == 0x85u && gtpu_packet[13] == 0x10u && gtpu_packet[14] == 0x01u,
                     "expected PDU Session Container header");
  mini_gnb_c_require(memcmp(gtpu_packet + 16u, inner_packet, inner_packet_length) == 0,
                     "expected inner packet inside G-PDU");
  mini_gnb_c_require(mini_gnb_c_gtpu_extract_gpdu(gtpu_packet,
                                                  gtpu_packet_length,
                                                  &extracted_teid,
                                                  &extracted_qfi,
                                                  extracted_inner_packet,
                                                  sizeof(extracted_inner_packet),
                                                  &extracted_inner_packet_length) == 0,
                     "expected G-PDU extraction");
  mini_gnb_c_require(extracted_teid == session.upf_teid, "expected extracted TEID");
  mini_gnb_c_require(extracted_qfi == session.qfi, "expected extracted QFI");
  mini_gnb_c_require(extracted_inner_packet_length == inner_packet_length, "expected extracted inner length");
  mini_gnb_c_require(memcmp(extracted_inner_packet, inner_packet, inner_packet_length) == 0,
                     "expected extracted inner packet bytes");
}

void test_gtpu_builders_reject_missing_state(void) {
  mini_gnb_c_core_session_t session;
  uint8_t packet[64];
  size_t packet_length = 0u;

  mini_gnb_c_core_session_reset(&session);
  memset(packet, 0, sizeof(packet));
  mini_gnb_c_require(mini_gnb_c_gtpu_build_ipv4_udp_probe(&session,
                                                          "10.45.0.1",
                                                          packet,
                                                          sizeof(packet),
                                                          &packet_length) != 0,
                     "expected IPv4/UDP probe rejection without UE IPv4");
  mini_gnb_c_require(mini_gnb_c_gtpu_build_echo_request(1u, packet, 8u, &packet_length) != 0,
                     "expected Echo Request capacity rejection");
  mini_gnb_c_require(mini_gnb_c_gtpu_validate_echo_response(packet, sizeof(packet), 1u) != 0,
                     "expected Echo Response validation failure on garbage input");

  mini_gnb_c_fill_test_session(&session);
  session.qfi_valid = false;
  mini_gnb_c_require(mini_gnb_c_gtpu_build_gpdu(&session,
                                                packet,
                                                1u,
                                                packet,
                                                sizeof(packet),
                                                &packet_length) != 0,
                     "expected G-PDU rejection without complete session state");
  mini_gnb_c_require(mini_gnb_c_gtpu_extract_gpdu(packet,
                                                  sizeof(packet),
                                                  NULL,
                                                  NULL,
                                                  packet,
                                                  sizeof(packet),
                                                  &packet_length) != 0,
                     "expected G-PDU extraction rejection on garbage input");
}
