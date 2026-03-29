#include "mini_gnb_c/phy_ul/mock_msg3_receiver.h"

#include <string.h>

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
                                          const mini_gnb_c_radio_burst_t* burst,
                                          mini_gnb_c_msg3_decode_indication_t* out_msg3) {
  if (receiver == NULL || slot == NULL || ul_grant == NULL || burst == NULL || out_msg3 == NULL) {
    return false;
  }

  if (slot->abs_slot != ul_grant->abs_slot || burst->ul_type != MINI_GNB_C_UL_BURST_MSG3 || burst->nof_samples == 0U) {
    return false;
  }

  /* When the mock UL transport carries an explicit RNTI, it must match the
   * currently scheduled TC-RNTI. Otherwise the Msg3 belongs to a different
   * random-access attempt and must be rejected. */
  if (burst->rnti != 0U && burst->rnti != ul_grant->tc_rnti) {
    return false;
  }

  memset(out_msg3, 0, sizeof(*out_msg3));
  out_msg3->sfn = slot->sfn;
  out_msg3->slot = slot->slot;
  out_msg3->abs_slot = slot->abs_slot;
  out_msg3->rnti = ul_grant->tc_rnti;
  out_msg3->crc_ok = burst->crc_ok_override_valid ? burst->crc_ok_override : receiver->config.msg3_crc_ok;
  out_msg3->snr_db = burst->snr_db;
  out_msg3->evm = burst->evm;
  out_msg3->mac_pdu = burst->mac_pdu;
  return true;
}
