#ifndef MINI_GNB_C_RLC_RLC_LITE_H
#define MINI_GNB_C_RLC_RLC_LITE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mini_gnb_c/common/types.h"

#define MINI_GNB_C_RLC_LITE_MAGIC 0xA5u
#define MINI_GNB_C_RLC_LITE_FLAG_START 0x01u
#define MINI_GNB_C_RLC_LITE_FLAG_END 0x02u
#define MINI_GNB_C_RLC_LITE_HEADER_LEN 8u

typedef struct {
  bool active;
  uint16_t sdu_id;
  uint16_t total_length;
  uint16_t received_length;
  mini_gnb_c_buffer_t sdu;
} mini_gnb_c_rlc_lite_receiver_t;

void mini_gnb_c_rlc_lite_receiver_init(mini_gnb_c_rlc_lite_receiver_t* receiver);
bool mini_gnb_c_rlc_lite_is_segment(const uint8_t* pdu, size_t pdu_length);
size_t mini_gnb_c_rlc_lite_segment_payload_capacity(size_t max_pdu_bytes);
int mini_gnb_c_rlc_lite_build_segment(uint16_t sdu_id,
                                      const uint8_t* sdu,
                                      size_t total_length,
                                      size_t offset,
                                      size_t max_pdu_bytes,
                                      mini_gnb_c_buffer_t* out_segment,
                                      size_t* out_consumed_bytes,
                                      bool* out_is_last);
int mini_gnb_c_rlc_lite_receiver_consume(mini_gnb_c_rlc_lite_receiver_t* receiver,
                                         const uint8_t* pdu,
                                         size_t pdu_length,
                                         mini_gnb_c_buffer_t* out_sdu,
                                         size_t* out_consumed_bytes);

#endif
