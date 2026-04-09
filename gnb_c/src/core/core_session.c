#include "mini_gnb_c/core/core_session.h"

#include <stdio.h>
#include <string.h>

void mini_gnb_c_core_session_reset(mini_gnb_c_core_session_t* session) {
  if (session == NULL) {
    return;
  }

  memset(session, 0, sizeof(*session));
}

void mini_gnb_c_core_session_set_c_rnti(mini_gnb_c_core_session_t* session, const uint16_t c_rnti) {
  if (session == NULL) {
    return;
  }

  session->c_rnti = c_rnti;
}

void mini_gnb_c_core_session_set_ran_ue_ngap_id(mini_gnb_c_core_session_t* session, const uint16_t ran_ue_ngap_id) {
  if (session == NULL) {
    return;
  }

  session->ran_ue_ngap_id = ran_ue_ngap_id;
  session->ran_ue_ngap_id_valid = true;
}

void mini_gnb_c_core_session_set_amf_ue_ngap_id(mini_gnb_c_core_session_t* session, const uint16_t amf_ue_ngap_id) {
  if (session == NULL) {
    return;
  }

  session->amf_ue_ngap_id = amf_ue_ngap_id;
  session->amf_ue_ngap_id_valid = true;
}

int mini_gnb_c_core_session_set_pdu_session_id(mini_gnb_c_core_session_t* session, const uint8_t pdu_session_id) {
  if (session == NULL || pdu_session_id == 0u) {
    return -1;
  }

  session->pdu_session_id = pdu_session_id;
  session->pdu_session_id_valid = true;
  return 0;
}

int mini_gnb_c_core_session_set_upf_tunnel(mini_gnb_c_core_session_t* session,
                                           const char* upf_ip,
                                           const uint32_t upf_teid) {
  if (session == NULL || upf_ip == NULL || upf_ip[0] == '\0') {
    return -1;
  }
  if (snprintf(session->upf_ip, sizeof(session->upf_ip), "%s", upf_ip) >= (int)sizeof(session->upf_ip)) {
    return -1;
  }

  session->upf_teid = upf_teid;
  session->upf_tunnel_valid = true;
  return 0;
}

int mini_gnb_c_core_session_set_qfi(mini_gnb_c_core_session_t* session, const uint8_t qfi) {
  if (session == NULL || qfi == 0u || qfi > 63u) {
    return -1;
  }

  session->qfi = qfi;
  session->qfi_valid = true;
  return 0;
}

void mini_gnb_c_core_session_set_ue_ipv4(mini_gnb_c_core_session_t* session, const uint8_t ue_ipv4[4]) {
  if (session == NULL || ue_ipv4 == NULL) {
    return;
  }

  memcpy(session->ue_ipv4, ue_ipv4, sizeof(session->ue_ipv4));
  session->ue_ipv4_valid = true;
}

int mini_gnb_c_core_session_format_ue_ipv4(const mini_gnb_c_core_session_t* session, char* out, const size_t out_size) {
  if (session == NULL || out == NULL || out_size == 0U || !session->ue_ipv4_valid) {
    return -1;
  }

  return snprintf(out,
                  out_size,
                  "%u.%u.%u.%u",
                  session->ue_ipv4[0],
                  session->ue_ipv4[1],
                  session->ue_ipv4[2],
                  session->ue_ipv4[3]) < (int)out_size
             ? 0
             : -1;
}

bool mini_gnb_c_core_session_has_user_plane(const mini_gnb_c_core_session_t* session) {
  return session != NULL && session->upf_tunnel_valid && session->qfi_valid && session->ue_ipv4_valid;
}

void mini_gnb_c_core_session_increment_uplink_nas_count(mini_gnb_c_core_session_t* session) {
  if (session == NULL) {
    return;
  }

  ++session->uplink_nas_count;
}

void mini_gnb_c_core_session_increment_downlink_nas_count(mini_gnb_c_core_session_t* session) {
  if (session == NULL) {
    return;
  }

  ++session->downlink_nas_count;
}
