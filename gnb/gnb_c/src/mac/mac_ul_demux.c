#include "mini_gnb_c/mac/mac_ul_demux.h"

#include <string.h>

void mini_gnb_c_mac_ul_demux_parse(const mini_gnb_c_buffer_t* mac_pdu,
                                   mini_gnb_c_mac_ul_parse_result_t* out_result) {
  size_t index = 0;

  if (mac_pdu == NULL || out_result == NULL) {
    return;
  }

  memset(out_result, 0, sizeof(*out_result));
  out_result->parse_ok = true;

  while (index < mac_pdu->len) {
    uint8_t lcid = 0;
    uint8_t length = 0;

    if ((index + 2U) > mac_pdu->len) {
      out_result->parse_ok = false;
      break;
    }

    lcid = mac_pdu->bytes[index];
    length = mac_pdu->bytes[index + 1U];
    if (out_result->lcid_count < MINI_GNB_C_MAX_LCID_SEQUENCE) {
      out_result->lcid_sequence[out_result->lcid_count++] = lcid;
    }

    if ((index + 2U + length) > mac_pdu->len) {
      out_result->parse_ok = false;
      break;
    }

    if (lcid == 1U) {
      out_result->has_ul_ccch = true;
      (void)mini_gnb_c_buffer_set_bytes(&out_result->ul_ccch_sdu,
                                        &mac_pdu->bytes[index + 2U],
                                        length);
    } else if (lcid == 2U) {
      if (length != 2U) {
        out_result->parse_ok = false;
        break;
      }
      out_result->has_crnti_ce = true;
      out_result->crnti_ce = (uint16_t)mac_pdu->bytes[index + 2U] |
                             ((uint16_t)mac_pdu->bytes[index + 3U] << 8U);
    }

    index += 2U + length;
  }
}
