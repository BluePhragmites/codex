#include "mini_gnb_c/rrc/rrc_ccch_stub.h"

#include <stdio.h>
#include <string.h>

void mini_gnb_c_parse_rrc_setup_request(const mini_gnb_c_buffer_t* ccch_sdu,
                                        mini_gnb_c_rrc_setup_request_info_t* out_request) {
  if (ccch_sdu == NULL || out_request == NULL) {
    return;
  }

  memset(out_request, 0, sizeof(*out_request));
  if (ccch_sdu->len < 8U) {
    out_request->valid = false;
    return;
  }

  memcpy(out_request->contention_id48, ccch_sdu->bytes, 6U);
  out_request->establishment_cause = ccch_sdu->bytes[6];
  out_request->ue_identity_type = ccch_sdu->bytes[7];
  (void)mini_gnb_c_buffer_set_bytes(&out_request->ue_identity_raw,
                                    &ccch_sdu->bytes[8],
                                    ccch_sdu->len - 8U);
  out_request->valid = true;
}

void mini_gnb_c_build_rrc_setup(const mini_gnb_c_rrc_setup_request_info_t* request,
                                const mini_gnb_c_sim_config_t* sim,
                                mini_gnb_c_rrc_setup_blob_t* out_setup) {
  char text[MINI_GNB_C_MAX_PAYLOAD];
  if (request == NULL || sim == NULL || out_setup == NULL) {
    return;
  }

  (void)snprintf(text,
                 sizeof(text),
                 "RRCSetup|cause=%u|ue_type=%u|sr_period_slots=%d|sr_offset_slot=%d",
                 request->establishment_cause,
                 request->ue_identity_type,
                 sim->post_msg4_sr_period_slots,
                 sim->post_msg4_sr_offset_slot);
  (void)mini_gnb_c_buffer_set_text(&out_setup->asn1_buf, text);
}
