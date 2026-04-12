#include "mini_gnb_c/radio/air_pdu.h"

#include <string.h>

enum {
  MINI_GNB_C_AIR_PDU_OFFSET_MAGIC = 0,
  MINI_GNB_C_AIR_PDU_OFFSET_VERSION = 2,
  MINI_GNB_C_AIR_PDU_OFFSET_TYPE = 3,
  MINI_GNB_C_AIR_PDU_OFFSET_FLAGS = 4,
  MINI_GNB_C_AIR_PDU_OFFSET_ABS_SLOT = 6,
  MINI_GNB_C_AIR_PDU_OFFSET_RNTI = 10,
  MINI_GNB_C_AIR_PDU_OFFSET_PREAMBLE = 12,
  MINI_GNB_C_AIR_PDU_OFFSET_RESERVED = 13,
  MINI_GNB_C_AIR_PDU_OFFSET_PAYLOAD_LEN = 14,
  MINI_GNB_C_AIR_PDU_OFFSET_CRC32 = 16
};

static void mini_gnb_c_air_write_u16(uint8_t* out, const uint16_t value) {
  out[0] = (uint8_t)(value & 0xffu);
  out[1] = (uint8_t)((value >> 8u) & 0xffu);
}

static void mini_gnb_c_air_write_u32(uint8_t* out, const uint32_t value) {
  out[0] = (uint8_t)(value & 0xffu);
  out[1] = (uint8_t)((value >> 8u) & 0xffu);
  out[2] = (uint8_t)((value >> 16u) & 0xffu);
  out[3] = (uint8_t)((value >> 24u) & 0xffu);
}

static uint16_t mini_gnb_c_air_read_u16(const uint8_t* in) {
  return (uint16_t)in[0] | ((uint16_t)in[1] << 8u);
}

static uint32_t mini_gnb_c_air_read_u32(const uint8_t* in) {
  return (uint32_t)in[0] | ((uint32_t)in[1] << 8u) | ((uint32_t)in[2] << 16u) | ((uint32_t)in[3] << 24u);
}

static uint32_t mini_gnb_c_air_crc32_update(uint32_t crc, const uint8_t* data, const size_t length) {
  size_t index = 0u;

  if (data == NULL && length > 0u) {
    return crc;
  }
  for (index = 0u; index < length; ++index) {
    uint32_t byte = data[index];
    int bit = 0;

    crc ^= byte;
    for (bit = 0; bit < 8; ++bit) {
      const uint32_t mask = (crc & 1u) != 0u ? 0xedb88320u : 0u;
      crc = (crc >> 1u) ^ mask;
    }
  }
  return crc;
}

static int mini_gnb_c_air_pdu_type_valid(const mini_gnb_c_air_pdu_type_t type) {
  switch (type) {
    case MINI_GNB_C_AIR_PDU_DL_SSB:
    case MINI_GNB_C_AIR_PDU_DL_CTRL:
    case MINI_GNB_C_AIR_PDU_DL_DATA:
    case MINI_GNB_C_AIR_PDU_UL_PRACH:
    case MINI_GNB_C_AIR_PDU_UL_DATA:
    case MINI_GNB_C_AIR_PDU_UL_ACK:
    case MINI_GNB_C_AIR_PDU_UL_SR:
      return 1;
  }
  return 0;
}

const char* mini_gnb_c_air_pdu_type_to_string(const mini_gnb_c_air_pdu_type_t type) {
  switch (type) {
    case MINI_GNB_C_AIR_PDU_DL_SSB:
      return "DL_SSB";
    case MINI_GNB_C_AIR_PDU_DL_CTRL:
      return "DL_CTRL";
    case MINI_GNB_C_AIR_PDU_DL_DATA:
      return "DL_DATA";
    case MINI_GNB_C_AIR_PDU_UL_PRACH:
      return "UL_PRACH";
    case MINI_GNB_C_AIR_PDU_UL_DATA:
      return "UL_DATA";
    case MINI_GNB_C_AIR_PDU_UL_ACK:
      return "UL_ACK";
    case MINI_GNB_C_AIR_PDU_UL_SR:
      return "UL_SR";
  }
  return "UNKNOWN";
}

uint32_t mini_gnb_c_air_crc32(const uint8_t* data, const size_t length) {
  uint32_t crc = 0xffffffffu;

  if (data == NULL && length > 0u) {
    return 0u;
  }
  crc = mini_gnb_c_air_crc32_update(crc, data, length);
  return ~crc;
}

int mini_gnb_c_air_pdu_build(const mini_gnb_c_air_pdu_build_request_t* request,
                             uint8_t* out,
                             const size_t out_capacity,
                             size_t* out_length) {
  uint32_t crc32 = 0u;
  size_t encoded_length = MINI_GNB_C_AIR_PDU_HEADER_LEN;

  if (out_length == NULL) {
    return -1;
  }
  *out_length = 0u;
  if (request == NULL || out == NULL) {
    return -1;
  }
  if (!mini_gnb_c_air_pdu_type_valid(request->type)) {
    return -1;
  }
  if (request->payload_length > 0u && request->payload == NULL) {
    return -1;
  }
  encoded_length += request->payload_length;
  if (out_capacity < encoded_length) {
    return -1;
  }

  memset(out, 0, encoded_length);
  mini_gnb_c_air_write_u16(out + MINI_GNB_C_AIR_PDU_OFFSET_MAGIC, MINI_GNB_C_AIR_PDU_MAGIC);
  out[MINI_GNB_C_AIR_PDU_OFFSET_VERSION] = MINI_GNB_C_AIR_PDU_VERSION;
  out[MINI_GNB_C_AIR_PDU_OFFSET_TYPE] = (uint8_t)request->type;
  mini_gnb_c_air_write_u16(out + MINI_GNB_C_AIR_PDU_OFFSET_FLAGS, request->flags);
  mini_gnb_c_air_write_u32(out + MINI_GNB_C_AIR_PDU_OFFSET_ABS_SLOT, request->abs_slot);
  mini_gnb_c_air_write_u16(out + MINI_GNB_C_AIR_PDU_OFFSET_RNTI, request->rnti);
  out[MINI_GNB_C_AIR_PDU_OFFSET_PREAMBLE] = request->preamble_id;
  out[MINI_GNB_C_AIR_PDU_OFFSET_RESERVED] = 0u;
  mini_gnb_c_air_write_u16(out + MINI_GNB_C_AIR_PDU_OFFSET_PAYLOAD_LEN, request->payload_length);
  if (request->payload_length > 0u) {
    memcpy(out + MINI_GNB_C_AIR_PDU_HEADER_LEN, request->payload, request->payload_length);
  }
  crc32 = mini_gnb_c_air_crc32_update(0xffffffffu, out, MINI_GNB_C_AIR_PDU_OFFSET_CRC32);
  if (request->payload_length > 0u) {
    crc32 = mini_gnb_c_air_crc32_update(crc32, request->payload, request->payload_length);
  }
  crc32 = ~crc32;
  mini_gnb_c_air_write_u32(out + MINI_GNB_C_AIR_PDU_OFFSET_CRC32, crc32);
  *out_length = encoded_length;
  return 0;
}

int mini_gnb_c_air_pdu_parse(const uint8_t* pdu,
                             const size_t pdu_length,
                             mini_gnb_c_air_pdu_view_t* out_view) {
  uint16_t payload_length = 0u;
  uint32_t expected_crc32 = 0u;
  uint32_t computed_crc32 = 0u;

  if (pdu == NULL || out_view == NULL || pdu_length < MINI_GNB_C_AIR_PDU_HEADER_LEN) {
    return -1;
  }
  if (mini_gnb_c_air_read_u16(pdu + MINI_GNB_C_AIR_PDU_OFFSET_MAGIC) != MINI_GNB_C_AIR_PDU_MAGIC) {
    return -1;
  }
  if (pdu[MINI_GNB_C_AIR_PDU_OFFSET_VERSION] != MINI_GNB_C_AIR_PDU_VERSION) {
    return -1;
  }
  if (!mini_gnb_c_air_pdu_type_valid((mini_gnb_c_air_pdu_type_t)pdu[MINI_GNB_C_AIR_PDU_OFFSET_TYPE])) {
    return -1;
  }

  payload_length = mini_gnb_c_air_read_u16(pdu + MINI_GNB_C_AIR_PDU_OFFSET_PAYLOAD_LEN);
  if (pdu_length != (size_t)MINI_GNB_C_AIR_PDU_HEADER_LEN + payload_length) {
    return -1;
  }

  expected_crc32 = mini_gnb_c_air_read_u32(pdu + MINI_GNB_C_AIR_PDU_OFFSET_CRC32);
  computed_crc32 = mini_gnb_c_air_crc32_update(0xffffffffu, pdu, MINI_GNB_C_AIR_PDU_OFFSET_CRC32);
  if (payload_length > 0u) {
    computed_crc32 = mini_gnb_c_air_crc32_update(computed_crc32, pdu + MINI_GNB_C_AIR_PDU_HEADER_LEN, payload_length);
  }
  computed_crc32 = ~computed_crc32;
  if (computed_crc32 != expected_crc32) {
    return -1;
  }

  memset(out_view, 0, sizeof(*out_view));
  out_view->type = (mini_gnb_c_air_pdu_type_t)pdu[MINI_GNB_C_AIR_PDU_OFFSET_TYPE];
  out_view->flags = mini_gnb_c_air_read_u16(pdu + MINI_GNB_C_AIR_PDU_OFFSET_FLAGS);
  out_view->abs_slot = mini_gnb_c_air_read_u32(pdu + MINI_GNB_C_AIR_PDU_OFFSET_ABS_SLOT);
  out_view->rnti = mini_gnb_c_air_read_u16(pdu + MINI_GNB_C_AIR_PDU_OFFSET_RNTI);
  out_view->preamble_id = pdu[MINI_GNB_C_AIR_PDU_OFFSET_PREAMBLE];
  out_view->payload_length = payload_length;
  out_view->crc32 = expected_crc32;
  out_view->payload = payload_length > 0u ? pdu + MINI_GNB_C_AIR_PDU_HEADER_LEN : NULL;
  return 0;
}
