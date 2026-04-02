#ifndef MINI_GNB_C_NGAP_NGAP_RUNTIME_H
#define MINI_GNB_C_NGAP_NGAP_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#include "mini_gnb_c/core/core_session.h"

int mini_gnb_c_ngap_build_ng_setup_request(uint8_t* message,
                                           size_t message_capacity,
                                           size_t* message_length);

int mini_gnb_c_ngap_build_initial_ue_message(const uint8_t* nas_pdu,
                                             size_t nas_pdu_length,
                                             uint16_t ran_ue_ngap_id,
                                             uint8_t* message,
                                             size_t message_capacity,
                                             size_t* message_length);

int mini_gnb_c_ngap_build_uplink_nas_transport(uint16_t amf_ue_ngap_id,
                                               uint16_t ran_ue_ngap_id,
                                               const uint8_t* nas_pdu,
                                               size_t nas_pdu_length,
                                               uint8_t* message,
                                               size_t message_capacity,
                                               size_t* message_length);

int mini_gnb_c_ngap_build_downlink_nas_transport(uint16_t amf_ue_ngap_id,
                                                 uint16_t ran_ue_ngap_id,
                                                 const uint8_t* nas_pdu,
                                                 size_t nas_pdu_length,
                                                 uint8_t* message,
                                                 size_t message_capacity,
                                                 size_t* message_length);

int mini_gnb_c_ngap_build_initial_context_setup_response(uint16_t amf_ue_ngap_id,
                                                         uint16_t ran_ue_ngap_id,
                                                         uint8_t* message,
                                                         size_t message_capacity,
                                                         size_t* message_length);

int mini_gnb_c_ngap_build_pdu_session_resource_setup_response(uint16_t amf_ue_ngap_id,
                                                              uint16_t ran_ue_ngap_id,
                                                              uint8_t* message,
                                                              size_t message_capacity,
                                                              size_t* message_length);

int mini_gnb_c_ngap_extract_amf_ue_ngap_id(const uint8_t* message,
                                           size_t message_length,
                                           uint16_t* amf_ue_ngap_id_out);

int mini_gnb_c_ngap_extract_nas_pdu(const uint8_t* message,
                                    size_t message_length,
                                    uint8_t* nas_pdu_out,
                                    size_t nas_pdu_capacity,
                                    size_t* nas_pdu_length_out);

int mini_gnb_c_ngap_extract_open5gs_user_plane_state(const uint8_t* message,
                                                     size_t message_length,
                                                     mini_gnb_c_core_session_t* core_session);

#endif
