#include "mini_gnb_c/common/types.h"

#include <stdio.h>
#include <string.h>

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
  }
  return "UL_BURST_UNKNOWN";
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
