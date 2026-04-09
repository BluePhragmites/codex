#include "mini_gnb_c/rlc/rlc_lite.h"

#include <string.h>

static uint16_t mini_gnb_c_rlc_lite_read_u16(const uint8_t* data) {
  return (uint16_t)((uint16_t)data[0] << 8u) | (uint16_t)data[1];
}

static void mini_gnb_c_rlc_lite_write_u16(uint8_t* out, const uint16_t value) {
  out[0] = (uint8_t)((value >> 8u) & 0xffu);
  out[1] = (uint8_t)(value & 0xffu);
}

void mini_gnb_c_rlc_lite_receiver_init(mini_gnb_c_rlc_lite_receiver_t* receiver) {
  if (receiver == NULL) {
    return;
  }
  memset(receiver, 0, sizeof(*receiver));
}

bool mini_gnb_c_rlc_lite_is_segment(const uint8_t* pdu, const size_t pdu_length) {
  return pdu != NULL && pdu_length >= MINI_GNB_C_RLC_LITE_HEADER_LEN && pdu[0] == MINI_GNB_C_RLC_LITE_MAGIC;
}

size_t mini_gnb_c_rlc_lite_segment_payload_capacity(const size_t max_pdu_bytes) {
  return max_pdu_bytes > MINI_GNB_C_RLC_LITE_HEADER_LEN ? max_pdu_bytes - MINI_GNB_C_RLC_LITE_HEADER_LEN : 0u;
}

int mini_gnb_c_rlc_lite_build_segment(const uint16_t sdu_id,
                                      const uint8_t* sdu,
                                      const size_t total_length,
                                      const size_t offset,
                                      const size_t max_pdu_bytes,
                                      mini_gnb_c_buffer_t* out_segment,
                                      size_t* out_consumed_bytes,
                                      bool* out_is_last) {
  const size_t payload_capacity = mini_gnb_c_rlc_lite_segment_payload_capacity(max_pdu_bytes);
  const size_t remaining_bytes = total_length > offset ? total_length - offset : 0u;
  const size_t segment_payload_bytes = remaining_bytes < payload_capacity ? remaining_bytes : payload_capacity;
  const bool is_start = offset == 0u;
  const bool is_last = segment_payload_bytes == remaining_bytes;
  uint8_t flags = 0u;

  if (out_consumed_bytes != NULL) {
    *out_consumed_bytes = 0u;
  }
  if (out_is_last != NULL) {
    *out_is_last = false;
  }
  if (out_segment == NULL || sdu == NULL || total_length == 0u || total_length > MINI_GNB_C_MAX_PAYLOAD ||
      offset >= total_length || payload_capacity == 0u ||
      total_length > UINT16_MAX || offset > UINT16_MAX) {
    return -1;
  }

  mini_gnb_c_buffer_reset(out_segment);
  if (segment_payload_bytes == 0u ||
      segment_payload_bytes + MINI_GNB_C_RLC_LITE_HEADER_LEN > sizeof(out_segment->bytes) ||
      offset + segment_payload_bytes > total_length) {
    return -1;
  }

  if (is_start) {
    flags |= MINI_GNB_C_RLC_LITE_FLAG_START;
  }
  if (is_last) {
    flags |= MINI_GNB_C_RLC_LITE_FLAG_END;
  }

  out_segment->bytes[0] = MINI_GNB_C_RLC_LITE_MAGIC;
  out_segment->bytes[1] = flags;
  mini_gnb_c_rlc_lite_write_u16(&out_segment->bytes[2], sdu_id);
  mini_gnb_c_rlc_lite_write_u16(&out_segment->bytes[4], (uint16_t)total_length);
  mini_gnb_c_rlc_lite_write_u16(&out_segment->bytes[6], (uint16_t)offset);
  memcpy(&out_segment->bytes[MINI_GNB_C_RLC_LITE_HEADER_LEN], &sdu[offset], segment_payload_bytes);
  out_segment->len = MINI_GNB_C_RLC_LITE_HEADER_LEN + segment_payload_bytes;

  if (out_consumed_bytes != NULL) {
    *out_consumed_bytes = segment_payload_bytes;
  }
  if (out_is_last != NULL) {
    *out_is_last = is_last;
  }
  return 0;
}

int mini_gnb_c_rlc_lite_receiver_consume(mini_gnb_c_rlc_lite_receiver_t* receiver,
                                         const uint8_t* pdu,
                                         const size_t pdu_length,
                                         mini_gnb_c_buffer_t* out_sdu,
                                         size_t* out_consumed_bytes) {
  const uint8_t flags =
      mini_gnb_c_rlc_lite_is_segment(pdu, pdu_length) ? pdu[1] : 0u;
  const bool is_start = (flags & MINI_GNB_C_RLC_LITE_FLAG_START) != 0u;
  const bool is_last = (flags & MINI_GNB_C_RLC_LITE_FLAG_END) != 0u;
  const uint16_t sdu_id =
      mini_gnb_c_rlc_lite_is_segment(pdu, pdu_length) ? mini_gnb_c_rlc_lite_read_u16(&pdu[2]) : 0u;
  const uint16_t total_length =
      mini_gnb_c_rlc_lite_is_segment(pdu, pdu_length) ? mini_gnb_c_rlc_lite_read_u16(&pdu[4]) : 0u;
  const uint16_t offset =
      mini_gnb_c_rlc_lite_is_segment(pdu, pdu_length) ? mini_gnb_c_rlc_lite_read_u16(&pdu[6]) : 0u;
  const size_t payload_length = pdu_length >= MINI_GNB_C_RLC_LITE_HEADER_LEN ? pdu_length - MINI_GNB_C_RLC_LITE_HEADER_LEN : 0u;

  if (out_sdu != NULL) {
    mini_gnb_c_buffer_reset(out_sdu);
  }
  if (out_consumed_bytes != NULL) {
    *out_consumed_bytes = 0u;
  }
  if (receiver == NULL || !mini_gnb_c_rlc_lite_is_segment(pdu, pdu_length) || total_length == 0u ||
      total_length > MINI_GNB_C_MAX_PAYLOAD || offset > total_length || payload_length > total_length ||
      (size_t)offset + payload_length > total_length) {
    return -1;
  }

  if (is_start) {
    mini_gnb_c_rlc_lite_receiver_init(receiver);
    receiver->active = true;
    receiver->sdu_id = sdu_id;
    receiver->total_length = total_length;
  } else if (!receiver->active || receiver->sdu_id != sdu_id || receiver->total_length != total_length) {
    return -1;
  }

  if (offset > receiver->received_length) {
    return -1;
  }
  if (offset < receiver->received_length) {
    if ((size_t)offset + payload_length > receiver->received_length) {
      return -1;
    }
  } else {
    memcpy(&receiver->sdu.bytes[offset], &pdu[MINI_GNB_C_RLC_LITE_HEADER_LEN], payload_length);
    receiver->received_length = (uint16_t)(offset + payload_length);
    receiver->sdu.len = receiver->received_length;
    if (out_consumed_bytes != NULL) {
      *out_consumed_bytes = payload_length;
    }
  }

  if (is_last) {
    if ((size_t)offset + payload_length != total_length || receiver->received_length != total_length) {
      return -1;
    }
    if (out_sdu != NULL) {
      *out_sdu = receiver->sdu;
    }
    mini_gnb_c_rlc_lite_receiver_init(receiver);
    return 1;
  }
  return 0;
}
