#include <string.h>

#include "test_helpers.h"

#include "mini_gnb_c/core/core_session.h"

void test_core_session_tracks_user_plane_state(void) {
  mini_gnb_c_core_session_t session;
  char ue_ipv4_text[MINI_GNB_C_CORE_MAX_IPV4_TEXT];
  const uint8_t ue_ipv4[4] = {10u, 45u, 0u, 7u};

  mini_gnb_c_core_session_reset(&session);
  mini_gnb_c_require(!mini_gnb_c_core_session_has_user_plane(&session),
                     "expected empty session to start without user-plane state");

  mini_gnb_c_core_session_set_c_rnti(&session, 0x4601u);
  mini_gnb_c_core_session_set_ran_ue_ngap_id(&session, 1u);
  mini_gnb_c_core_session_set_amf_ue_ngap_id(&session, 2u);
  mini_gnb_c_require(mini_gnb_c_core_session_set_pdu_session_id(&session, 1u) == 0,
                     "expected valid PDU session ID");
  mini_gnb_c_require(mini_gnb_c_core_session_set_upf_tunnel(&session, "127.0.0.7", 0x0000ef26u) == 0,
                     "expected valid UPF tunnel");
  mini_gnb_c_require(mini_gnb_c_core_session_set_qfi(&session, 1u) == 0, "expected valid QFI");
  mini_gnb_c_core_session_set_ue_ipv4(&session, ue_ipv4);
  mini_gnb_c_core_session_increment_uplink_nas_count(&session);
  mini_gnb_c_core_session_increment_downlink_nas_count(&session);

  mini_gnb_c_require(session.c_rnti == 0x4601u, "expected C-RNTI to be stored");
  mini_gnb_c_require(session.ran_ue_ngap_id_valid && session.ran_ue_ngap_id == 1u,
                     "expected RAN UE NGAP ID");
  mini_gnb_c_require(session.amf_ue_ngap_id_valid && session.amf_ue_ngap_id == 2u,
                     "expected AMF UE NGAP ID");
  mini_gnb_c_require(session.pdu_session_id_valid && session.pdu_session_id == 1u,
                     "expected PDU session ID");
  mini_gnb_c_require(strcmp(session.upf_ip, "127.0.0.7") == 0, "expected UPF IP");
  mini_gnb_c_require(session.upf_teid == 0x0000ef26u, "expected UPF TEID");
  mini_gnb_c_require(session.qfi_valid && session.qfi == 1u, "expected QFI");
  mini_gnb_c_require(session.uplink_nas_count == 1u, "expected uplink NAS counter");
  mini_gnb_c_require(session.downlink_nas_count == 1u, "expected downlink NAS counter");
  mini_gnb_c_require(mini_gnb_c_core_session_format_ue_ipv4(&session, ue_ipv4_text, sizeof(ue_ipv4_text)) == 0,
                     "expected UE IPv4 formatting");
  mini_gnb_c_require(strcmp(ue_ipv4_text, "10.45.0.7") == 0, "expected formatted UE IPv4");
  mini_gnb_c_require(mini_gnb_c_core_session_has_user_plane(&session),
                     "expected complete session user-plane state");
}

void test_core_session_rejects_invalid_values(void) {
  mini_gnb_c_core_session_t session;

  mini_gnb_c_core_session_reset(&session);
  mini_gnb_c_require(mini_gnb_c_core_session_set_pdu_session_id(&session, 0u) != 0,
                     "expected zero PDU session ID rejection");
  mini_gnb_c_require(mini_gnb_c_core_session_set_qfi(&session, 0u) != 0, "expected zero QFI rejection");
  mini_gnb_c_require(mini_gnb_c_core_session_set_qfi(&session, 64u) != 0, "expected QFI range rejection");
  mini_gnb_c_require(mini_gnb_c_core_session_set_upf_tunnel(&session, "", 1u) != 0,
                     "expected empty UPF IP rejection");
  mini_gnb_c_require(mini_gnb_c_core_session_format_ue_ipv4(&session, NULL, 0u) != 0,
                     "expected format rejection without UE IPv4");
}
