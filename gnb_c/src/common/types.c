#include "mini_gnb_c/common/types.h"

#include <stdio.h>
#include <string.h>

typedef struct {
  uint16_t prb_len;
  uint8_t mcs;
  uint16_t tbsize;
} mini_gnb_c_tbsize_entry_t;

static const mini_gnb_c_tbsize_entry_t mini_gnb_c_tbsize_table[] = {
    {8U, 4U, 16U},   {12U, 4U, 24U},  {16U, 4U, 32U},  {16U, 8U, 64U},  {20U, 0U, 24U},
    {20U, 8U, 80U},  {24U, 4U, 48U},  {24U, 8U, 96U},  {24U, 9U, 120U}, {28U, 8U, 112U},
    {32U, 8U, 128U}, {36U, 8U, 144U}, {40U, 8U, 160U},
};

const char* mini_gnb_c_dl_object_type_to_string(const mini_gnb_c_dl_object_type_t type) {
  switch (type) {
    case MINI_GNB_C_DL_OBJ_SSB:
      return "DL_OBJ_SSB";
    case MINI_GNB_C_DL_OBJ_SIB1:
      return "DL_OBJ_SIB1";
    case MINI_GNB_C_DL_OBJ_RAR:
      return "DL_OBJ_RAR";
    case MINI_GNB_C_DL_OBJ_MSG4:
      return "DL_OBJ_MSG4";
    case MINI_GNB_C_DL_OBJ_DATA:
      return "DL_OBJ_DATA";
    case MINI_GNB_C_DL_OBJ_PDCCH:
      return "DL_OBJ_PDCCH";
  }
  return "DL_OBJ_UNKNOWN";
}

const char* mini_gnb_c_ul_burst_type_to_string(const mini_gnb_c_ul_burst_type_t type) {
  switch (type) {
    case MINI_GNB_C_UL_BURST_NONE:
      return "UL_BURST_NONE";
    case MINI_GNB_C_UL_BURST_PRACH:
      return "UL_BURST_PRACH";
    case MINI_GNB_C_UL_BURST_MSG3:
      return "UL_BURST_MSG3";
    case MINI_GNB_C_UL_BURST_PUCCH_SR:
      return "UL_BURST_PUCCH_SR";
    case MINI_GNB_C_UL_BURST_DATA:
      return "UL_BURST_DATA";
    case MINI_GNB_C_UL_BURST_PUCCH_ACK:
      return "UL_BURST_PUCCH_ACK";
  }
  return "UL_BURST_UNKNOWN";
}

const char* mini_gnb_c_dci_format_to_string(const mini_gnb_c_dci_format_t format) {
  switch (format) {
    case MINI_GNB_C_DCI_FORMAT_0_0:
      return "DCI0_0";
    case MINI_GNB_C_DCI_FORMAT_0_1:
      return "DCI0_1";
    case MINI_GNB_C_DCI_FORMAT_1_0:
      return "DCI1_0";
    case MINI_GNB_C_DCI_FORMAT_1_1:
      return "DCI1_1";
  }
  return "DCI_UNKNOWN";
}

const char* mini_gnb_c_ra_state_to_string(const mini_gnb_c_ra_state_t state) {
  switch (state) {
    case MINI_GNB_C_RA_IDLE:
      return "IDLE";
    case MINI_GNB_C_RA_PRACH_DETECTED:
      return "PRACH_DETECTED";
    case MINI_GNB_C_RA_TC_RNTI_ASSIGNED:
      return "TC_RNTI_ASSIGNED";
    case MINI_GNB_C_RA_RAR_SENT:
      return "RAR_SENT";
    case MINI_GNB_C_RA_MSG3_WAIT:
      return "MSG3_WAIT";
    case MINI_GNB_C_RA_MSG3_OK:
      return "MSG3_OK";
    case MINI_GNB_C_RA_MSG4_SENT:
      return "MSG4_SENT";
    case MINI_GNB_C_RA_DONE:
      return "DONE";
    case MINI_GNB_C_RA_FAIL:
      return "FAIL";
  }
  return "UNKNOWN";
}

uint16_t mini_gnb_c_lookup_tbsize(const uint16_t prb_len, const uint8_t mcs) {
  size_t i = 0;

  for (i = 0; i < (sizeof(mini_gnb_c_tbsize_table) / sizeof(mini_gnb_c_tbsize_table[0])); ++i) {
    if (mini_gnb_c_tbsize_table[i].prb_len == prb_len && mini_gnb_c_tbsize_table[i].mcs == mcs) {
      return mini_gnb_c_tbsize_table[i].tbsize;
    }
  }

  return 0U;
}

void mini_gnb_c_buffer_reset(mini_gnb_c_buffer_t* buffer) {
  if (buffer == NULL) {
    return;
  }
  memset(buffer->bytes, 0, sizeof(buffer->bytes));
  buffer->len = 0;
}

int mini_gnb_c_buffer_set_bytes(mini_gnb_c_buffer_t* buffer, const uint8_t* data, const size_t len) {
  if (buffer == NULL || (data == NULL && len != 0U) || len > MINI_GNB_C_MAX_PAYLOAD) {
    return -1;
  }

  mini_gnb_c_buffer_reset(buffer);
  if (len != 0U) {
    memcpy(buffer->bytes, data, len);
  }
  buffer->len = len;
  return 0;
}

int mini_gnb_c_buffer_set_text(mini_gnb_c_buffer_t* buffer, const char* text) {
  size_t len = 0;
  if (text != NULL) {
    len = strlen(text);
  }
  return mini_gnb_c_buffer_set_bytes(buffer, (const uint8_t*)text, len);
}
