#ifndef MINI_GNB_C_RADIO_AIR_PDU_H
#define MINI_GNB_C_RADIO_AIR_PDU_H

#include <stddef.h>
#include <stdint.h>

#include "mini_gnb_c/common/types.h"

#define MINI_GNB_C_AIR_PDU_MAGIC 0x4d41u
#define MINI_GNB_C_AIR_PDU_VERSION 1u
#define MINI_GNB_C_AIR_PDU_HEADER_LEN 20u

typedef enum {
  MINI_GNB_C_AIR_PDU_DL_SSB = 1,
  MINI_GNB_C_AIR_PDU_DL_CTRL = 2,
  MINI_GNB_C_AIR_PDU_DL_DATA = 3,
  MINI_GNB_C_AIR_PDU_UL_PRACH = 4,
  MINI_GNB_C_AIR_PDU_UL_DATA = 5,
  MINI_GNB_C_AIR_PDU_UL_ACK = 6,
  MINI_GNB_C_AIR_PDU_UL_SR = 7
} mini_gnb_c_air_pdu_type_t;

typedef struct {
  mini_gnb_c_air_pdu_type_t type;
  uint16_t flags;
  uint32_t abs_slot;
  uint16_t rnti;
  uint8_t preamble_id;
  const uint8_t* payload;
  uint16_t payload_length;
} mini_gnb_c_air_pdu_build_request_t;

typedef struct {
  mini_gnb_c_air_pdu_type_t type;
  uint16_t flags;
  uint32_t abs_slot;
  uint16_t rnti;
  uint8_t preamble_id;
  uint32_t crc32;
  const uint8_t* payload;
  uint16_t payload_length;
} mini_gnb_c_air_pdu_view_t;

const char* mini_gnb_c_air_pdu_type_to_string(mini_gnb_c_air_pdu_type_t type);

uint32_t mini_gnb_c_air_crc32(const uint8_t* data, size_t length);

int mini_gnb_c_air_pdu_build(const mini_gnb_c_air_pdu_build_request_t* request,
                             uint8_t* out,
                             size_t out_capacity,
                             size_t* out_length);

int mini_gnb_c_air_pdu_parse(const uint8_t* pdu, size_t pdu_length, mini_gnb_c_air_pdu_view_t* out_view);

#endif
