#include "mini_gnb_c/phy_dl/rar_builder.h"

void mini_gnb_c_build_rar_pdu(const mini_gnb_c_ra_schedule_request_t* request,
                              mini_gnb_c_buffer_t* out_rar) {
  if (request == NULL || out_rar == NULL) {
    return;
  }

  mini_gnb_c_buffer_reset(out_rar);
  out_rar->bytes[0] = request->preamble_id;
  out_rar->bytes[1] = request->ta_cmd;
  out_rar->bytes[2] = (uint8_t)(request->ul_grant.msg3_prb_start & 0xFFU);
  out_rar->bytes[3] = (uint8_t)((request->ul_grant.msg3_prb_start >> 8U) & 0xFFU);
  out_rar->bytes[4] = (uint8_t)(request->ul_grant.msg3_prb_len & 0xFFU);
  out_rar->bytes[5] = (uint8_t)((request->ul_grant.msg3_prb_len >> 8U) & 0xFFU);
  out_rar->bytes[6] = request->ul_grant.msg3_mcs;
  out_rar->bytes[7] = request->ul_grant.k2;
  out_rar->bytes[8] = (uint8_t)(request->tc_rnti & 0xFFU);
  out_rar->bytes[9] = (uint8_t)((request->tc_rnti >> 8U) & 0xFFU);
  out_rar->bytes[10] = (uint8_t)(request->ul_grant.abs_slot & 0xFFU);
  out_rar->bytes[11] = (uint8_t)((request->ul_grant.abs_slot >> 8U) & 0xFFU);
  out_rar->len = 12;
}
