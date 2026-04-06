#ifndef MINI_GNB_C_NAS_NAS_5GS_MIN_H
#define MINI_GNB_C_NAS_NAS_5GS_MIN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mini_gnb_c/common/types.h"

#define MINI_GNB_C_NAS_5GS_MIN_MAX_PENDING 8u

typedef enum {
  MINI_GNB_C_NAS_5GS_MIN_UPLINK_IDENTITY_RESPONSE = 0,
  MINI_GNB_C_NAS_5GS_MIN_UPLINK_AUTHENTICATION_RESPONSE = 1,
  MINI_GNB_C_NAS_5GS_MIN_UPLINK_SECURITY_MODE_COMPLETE = 2,
  MINI_GNB_C_NAS_5GS_MIN_UPLINK_REGISTRATION_COMPLETE = 3,
  MINI_GNB_C_NAS_5GS_MIN_UPLINK_PDU_SESSION_REQUEST = 4
} mini_gnb_c_nas_5gs_min_uplink_kind_t;

typedef struct {
  bool valid;
  int abs_slot;
  mini_gnb_c_nas_5gs_min_uplink_kind_t kind;
  mini_gnb_c_buffer_t nas_pdu;
} mini_gnb_c_nas_5gs_min_pending_uplink_t;

typedef struct {
  uint8_t rand[16];
  uint8_t autn[16];
  uint8_t res[16];
  uint8_t res_star[16];
  size_t res_len;
  size_t res_star_len;
  uint8_t ck[16];
  uint8_t ik[16];
  uint8_t ak[6];
  uint8_t sqn[6];
  uint8_t abba[8];
  size_t abba_len;
  uint8_t kausf[32];
  uint8_t kseaf[32];
  uint8_t kamf[32];
  uint8_t knas_enc[16];
  uint8_t knas_int[16];
  uint8_t ciphering_algorithm;
  uint8_t integrity_algorithm;
} mini_gnb_c_nas_5gs_min_security_context_t;

typedef struct {
  uint32_t next_gnb_to_ue_sequence;
  uint32_t next_ue_to_gnb_nas_sequence;
  uint16_t c_rnti;
  bool c_rnti_valid;
  uint16_t ran_ue_ngap_id;
  bool ran_ue_ngap_id_valid;
  uint16_t amf_ue_ngap_id;
  bool amf_ue_ngap_id_valid;
  bool identity_response_pending;
  bool identity_response_sent;
  bool authentication_response_pending;
  bool authentication_response_sent;
  bool security_mode_complete_pending;
  bool security_mode_complete_sent;
  bool registration_complete_pending;
  bool registration_complete_sent;
  bool pdu_session_request_pending;
  bool pdu_session_request_sent;
  bool security_context_valid;
  mini_gnb_c_nas_5gs_min_security_context_t security_context;
  mini_gnb_c_nas_5gs_min_pending_uplink_t pending[MINI_GNB_C_NAS_5GS_MIN_MAX_PENDING];
} mini_gnb_c_nas_5gs_min_ue_t;

void mini_gnb_c_nas_5gs_min_ue_init(mini_gnb_c_nas_5gs_min_ue_t* runtime);
uint8_t mini_gnb_c_nas_5gs_min_extract_message_type(const uint8_t* nas_pdu, size_t nas_pdu_length);
int mini_gnb_c_nas_5gs_min_handle_downlink_nas(mini_gnb_c_nas_5gs_min_ue_t* runtime,
                                               uint16_t c_rnti,
                                               uint16_t ran_ue_ngap_id,
                                               bool ran_ue_ngap_id_valid,
                                               uint16_t amf_ue_ngap_id,
                                               bool amf_ue_ngap_id_valid,
                                               const uint8_t* nas_pdu,
                                               size_t nas_pdu_length,
                                               int current_slot);
int mini_gnb_c_nas_5gs_min_emit_due_uplinks(mini_gnb_c_nas_5gs_min_ue_t* runtime,
                                            const char* exchange_dir,
                                            int current_slot);
int mini_gnb_c_nas_5gs_min_poll_exchange(mini_gnb_c_nas_5gs_min_ue_t* runtime,
                                         const char* exchange_dir,
                                         int current_slot);

#endif
