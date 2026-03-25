#include "mini_gnb_c/phy_ul/mock_msg3_receiver.h"

#include <string.h>

#include "mini_gnb_c/common/hex.h"

void mini_gnb_c_mock_msg3_receiver_init(mini_gnb_c_mock_msg3_receiver_t* receiver,
                                        const mini_gnb_c_sim_config_t* config) {
  if (receiver == NULL || config == NULL) {
    return;
  }
  memset(receiver, 0, sizeof(*receiver));
  memcpy(&receiver->config, config, sizeof(*config));
}

bool mini_gnb_c_mock_msg3_receiver_decode(const mini_gnb_c_mock_msg3_receiver_t* receiver,
                                          const mini_gnb_c_slot_indication_t* slot,
                                          const mini_gnb_c_ul_grant_for_msg3_t* ul_grant,
                                          mini_gnb_c_msg3_decode_indication_t* out_msg3) {
  uint8_t contention_id[16];
  uint8_t ue_identity[16];
  size_t contention_id_len = 0;
  size_t ue_identity_len = 0;
  mini_gnb_c_buffer_t ccch;

  if (receiver == NULL || slot == NULL || ul_grant == NULL || out_msg3 == NULL) {
    return false;
  }

  if (mini_gnb_c_hex_to_bytes(receiver->config.contention_id_hex,
                              contention_id,
                              sizeof(contention_id),
                              &contention_id_len) != 0) {
    return false;
  }

  if (mini_gnb_c_hex_to_bytes(receiver->config.ue_identity_hex,
                              ue_identity,
                              sizeof(ue_identity),
                              &ue_identity_len) != 0) {
    return false;
  }

  mini_gnb_c_buffer_reset(&ccch);
  memcpy(ccch.bytes, contention_id, contention_id_len);
  ccch.bytes[contention_id_len] = receiver->config.establishment_cause;
  ccch.bytes[contention_id_len + 1U] = receiver->config.ue_identity_type;
  memcpy(&ccch.bytes[contention_id_len + 2U], ue_identity, ue_identity_len);
  ccch.len = contention_id_len + 2U + ue_identity_len;

  memset(out_msg3, 0, sizeof(*out_msg3));
  out_msg3->sfn = slot->sfn;
  out_msg3->slot = slot->slot;
  out_msg3->abs_slot = slot->abs_slot;
  out_msg3->rnti = ul_grant->tc_rnti;
  out_msg3->crc_ok = receiver->config.msg3_crc_ok;
  out_msg3->snr_db = receiver->config.msg3_snr_db;
  out_msg3->evm = receiver->config.msg3_evm;

  if (receiver->config.include_crnti_ce) {
    out_msg3->mac_pdu.bytes[out_msg3->mac_pdu.len++] = 2;
    out_msg3->mac_pdu.bytes[out_msg3->mac_pdu.len++] = 2;
    out_msg3->mac_pdu.bytes[out_msg3->mac_pdu.len++] = (uint8_t)(ul_grant->tc_rnti & 0xFFU);
    out_msg3->mac_pdu.bytes[out_msg3->mac_pdu.len++] = (uint8_t)((ul_grant->tc_rnti >> 8U) & 0xFFU);
  }

  out_msg3->mac_pdu.bytes[out_msg3->mac_pdu.len++] = 1;
  out_msg3->mac_pdu.bytes[out_msg3->mac_pdu.len++] = (uint8_t)ccch.len;
  memcpy(&out_msg3->mac_pdu.bytes[out_msg3->mac_pdu.len], ccch.bytes, ccch.len);
  out_msg3->mac_pdu.len += ccch.len;
  return true;
}
