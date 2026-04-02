#ifndef MINI_GNB_C_CORE_CORE_SESSION_H
#define MINI_GNB_C_CORE_CORE_SESSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MINI_GNB_C_CORE_MAX_IPV4_TEXT 16

typedef struct {
  uint16_t c_rnti;
  uint16_t ran_ue_ngap_id;
  uint16_t amf_ue_ngap_id;
  uint8_t pdu_session_id;
  char upf_ip[MINI_GNB_C_CORE_MAX_IPV4_TEXT];
  uint32_t upf_teid;
  uint8_t qfi;
  uint8_t ue_ipv4[4];
  bool ran_ue_ngap_id_valid;
  bool amf_ue_ngap_id_valid;
  bool pdu_session_id_valid;
  bool upf_tunnel_valid;
  bool qfi_valid;
  bool ue_ipv4_valid;
  uint32_t uplink_nas_count;
  uint32_t downlink_nas_count;
} mini_gnb_c_core_session_t;

void mini_gnb_c_core_session_reset(mini_gnb_c_core_session_t* session);
void mini_gnb_c_core_session_set_c_rnti(mini_gnb_c_core_session_t* session, uint16_t c_rnti);
void mini_gnb_c_core_session_set_ran_ue_ngap_id(mini_gnb_c_core_session_t* session, uint16_t ran_ue_ngap_id);
void mini_gnb_c_core_session_set_amf_ue_ngap_id(mini_gnb_c_core_session_t* session, uint16_t amf_ue_ngap_id);
int mini_gnb_c_core_session_set_pdu_session_id(mini_gnb_c_core_session_t* session, uint8_t pdu_session_id);
int mini_gnb_c_core_session_set_upf_tunnel(mini_gnb_c_core_session_t* session,
                                           const char* upf_ip,
                                           uint32_t upf_teid);
int mini_gnb_c_core_session_set_qfi(mini_gnb_c_core_session_t* session, uint8_t qfi);
void mini_gnb_c_core_session_set_ue_ipv4(mini_gnb_c_core_session_t* session, const uint8_t ue_ipv4[4]);
int mini_gnb_c_core_session_format_ue_ipv4(const mini_gnb_c_core_session_t* session, char* out, size_t out_size);
bool mini_gnb_c_core_session_has_user_plane(const mini_gnb_c_core_session_t* session);
void mini_gnb_c_core_session_increment_uplink_nas_count(mini_gnb_c_core_session_t* session);
void mini_gnb_c_core_session_increment_downlink_nas_count(mini_gnb_c_core_session_t* session);

#endif
