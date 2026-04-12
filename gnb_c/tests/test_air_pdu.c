#include <string.h>

#include "test_helpers.h"

#include "mini_gnb_c/radio/air_pdu.h"

void test_air_pdu_build_and_parse_round_trip(void) {
  static const uint8_t k_payload[] = {0x11u, 0x22u, 0x33u, 0x44u, 0x55u};
  mini_gnb_c_air_pdu_build_request_t request;
  mini_gnb_c_air_pdu_view_t view;
  uint8_t encoded[MINI_GNB_C_AIR_PDU_HEADER_LEN + sizeof(k_payload)];
  size_t encoded_length = 0u;

  memset(&request, 0, sizeof(request));
  memset(&view, 0, sizeof(view));
  request.type = MINI_GNB_C_AIR_PDU_DL_DATA;
  request.flags = 0x1234u;
  request.abs_slot = 321u;
  request.rnti = 0x4601u;
  request.preamble_id = 0u;
  request.payload = k_payload;
  request.payload_length = (uint16_t)sizeof(k_payload);

  mini_gnb_c_require(mini_gnb_c_air_pdu_build(&request, encoded, sizeof(encoded), &encoded_length) == 0,
                     "expected air PDU build success");
  mini_gnb_c_require(encoded_length == MINI_GNB_C_AIR_PDU_HEADER_LEN + sizeof(k_payload),
                     "expected encoded air PDU length");
  mini_gnb_c_require(mini_gnb_c_air_pdu_parse(encoded, encoded_length, &view) == 0,
                     "expected air PDU parse success");
  mini_gnb_c_require(view.type == MINI_GNB_C_AIR_PDU_DL_DATA, "expected DL_DATA type");
  mini_gnb_c_require(view.flags == 0x1234u, "expected flags round-trip");
  mini_gnb_c_require(view.abs_slot == 321u, "expected abs_slot round-trip");
  mini_gnb_c_require(view.rnti == 0x4601u, "expected rnti round-trip");
  mini_gnb_c_require(view.payload_length == sizeof(k_payload), "expected payload length round-trip");
  mini_gnb_c_require(view.payload != NULL, "expected payload pointer");
  mini_gnb_c_require(memcmp(view.payload, k_payload, sizeof(k_payload)) == 0, "expected payload round-trip");
}

void test_air_pdu_rejects_crc_mismatch(void) {
  static const uint8_t k_payload[] = {0xaau, 0xbbu, 0xccu};
  mini_gnb_c_air_pdu_build_request_t request;
  mini_gnb_c_air_pdu_view_t view;
  uint8_t encoded[MINI_GNB_C_AIR_PDU_HEADER_LEN + sizeof(k_payload)];
  size_t encoded_length = 0u;

  memset(&request, 0, sizeof(request));
  memset(&view, 0, sizeof(view));
  request.type = MINI_GNB_C_AIR_PDU_UL_DATA;
  request.abs_slot = 77u;
  request.rnti = 0x1234u;
  request.payload = k_payload;
  request.payload_length = (uint16_t)sizeof(k_payload);

  mini_gnb_c_require(mini_gnb_c_air_pdu_build(&request, encoded, sizeof(encoded), &encoded_length) == 0,
                     "expected air PDU build success");
  encoded[MINI_GNB_C_AIR_PDU_HEADER_LEN] ^= 0x01u;
  mini_gnb_c_require(mini_gnb_c_air_pdu_parse(encoded, encoded_length, &view) != 0,
                     "expected CRC mismatch rejection");
}

void test_air_pdu_rejects_invalid_header_fields(void) {
  uint8_t encoded[MINI_GNB_C_AIR_PDU_HEADER_LEN];
  mini_gnb_c_air_pdu_view_t view;

  memset(encoded, 0, sizeof(encoded));
  memset(&view, 0, sizeof(view));

  mini_gnb_c_require(mini_gnb_c_air_pdu_parse(encoded, sizeof(encoded), &view) != 0,
                     "expected invalid magic rejection");
  encoded[0] = (uint8_t)(MINI_GNB_C_AIR_PDU_MAGIC & 0xffu);
  encoded[1] = (uint8_t)((MINI_GNB_C_AIR_PDU_MAGIC >> 8u) & 0xffu);
  encoded[2] = MINI_GNB_C_AIR_PDU_VERSION;
  encoded[3] = 0xffu;
  mini_gnb_c_require(mini_gnb_c_air_pdu_parse(encoded, sizeof(encoded), &view) != 0,
                     "expected invalid type rejection");
}
