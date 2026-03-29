#include "mini_gnb_c/phy_dl/msg4_builder.h"

#include <string.h>

void mini_gnb_c_build_msg4_pdu(const mini_gnb_c_msg4_schedule_request_t* request,
                               mini_gnb_c_buffer_t* out_msg4) {
  if (request == NULL || out_msg4 == NULL) {
    return;
  }

  mini_gnb_c_buffer_reset(out_msg4);
  out_msg4->bytes[0] = 16;
  out_msg4->bytes[1] = 6;
  memcpy(&out_msg4->bytes[2], request->contention_id48, 6U);
  out_msg4->bytes[8] = 17;
  out_msg4->bytes[9] = (uint8_t)request->rrc_setup.asn1_buf.len;
  if (request->rrc_setup.asn1_buf.len > 0U) {
    memcpy(&out_msg4->bytes[10], request->rrc_setup.asn1_buf.bytes, request->rrc_setup.asn1_buf.len);
  }
  out_msg4->len = 10U + request->rrc_setup.asn1_buf.len;
}
