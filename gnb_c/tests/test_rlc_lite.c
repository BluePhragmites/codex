#include <string.h>

#include "test_helpers.h"

#include "mini_gnb_c/rlc/rlc_lite.h"

void test_rlc_lite_builds_and_reassembles_segmented_sdu(void) {
  uint8_t sdu[300];
  mini_gnb_c_buffer_t segment_a;
  mini_gnb_c_buffer_t segment_b;
  mini_gnb_c_buffer_t segment_c;
  mini_gnb_c_buffer_t reassembled;
  mini_gnb_c_rlc_lite_receiver_t receiver;
  size_t consumed = 0u;
  bool is_last = false;
  size_t i = 0u;

  for (i = 0u; i < sizeof(sdu); ++i) {
    sdu[i] = (uint8_t)(i & 0xffu);
  }

  mini_gnb_c_require(mini_gnb_c_rlc_lite_build_segment(7u,
                                                       sdu,
                                                       sizeof(sdu),
                                                       0u,
                                                       96u,
                                                       &segment_a,
                                                       &consumed,
                                                       &is_last) == 0,
                     "expected first segment");
  mini_gnb_c_require(consumed == 88u, "expected first segment payload bytes");
  mini_gnb_c_require(!is_last, "expected more segments after first one");

  mini_gnb_c_require(mini_gnb_c_rlc_lite_build_segment(7u,
                                                       sdu,
                                                       sizeof(sdu),
                                                       consumed,
                                                       96u,
                                                       &segment_b,
                                                       &i,
                                                       &is_last) == 0,
                     "expected second segment");
  mini_gnb_c_require(i == 88u, "expected second segment payload bytes");
  mini_gnb_c_require(!is_last, "expected more segments after second one");

  mini_gnb_c_require(mini_gnb_c_rlc_lite_build_segment(7u,
                                                       sdu,
                                                       sizeof(sdu),
                                                       consumed + i,
                                                       140u,
                                                       &segment_c,
                                                       &i,
                                                       &is_last) == 0,
                     "expected final segment");
  mini_gnb_c_require(i == 124u, "expected final segment payload bytes");
  mini_gnb_c_require(is_last, "expected final segment flag");

  mini_gnb_c_rlc_lite_receiver_init(&receiver);
  mini_gnb_c_require(mini_gnb_c_rlc_lite_receiver_consume(&receiver,
                                                          segment_a.bytes,
                                                          segment_a.len,
                                                          &reassembled,
                                                          &consumed) == 0,
                     "expected partial reassembly after first segment");
  mini_gnb_c_require(consumed == 88u, "expected consumed bytes after first segment");
  mini_gnb_c_require(mini_gnb_c_rlc_lite_receiver_consume(&receiver,
                                                          segment_b.bytes,
                                                          segment_b.len,
                                                          &reassembled,
                                                          &consumed) == 0,
                     "expected partial reassembly after second segment");
  mini_gnb_c_require(consumed == 88u, "expected consumed bytes after second segment");
  mini_gnb_c_require(mini_gnb_c_rlc_lite_receiver_consume(&receiver,
                                                          segment_c.bytes,
                                                          segment_c.len,
                                                          &reassembled,
                                                          &consumed) == 1,
                     "expected completed reassembly after final segment");
  mini_gnb_c_require(consumed == 124u, "expected consumed bytes after final segment");
  mini_gnb_c_require(reassembled.len == sizeof(sdu), "expected full reassembled SDU length");
  mini_gnb_c_require(memcmp(reassembled.bytes, sdu, sizeof(sdu)) == 0, "expected reassembled SDU payload");
}

void test_rlc_lite_rejects_out_of_order_segment(void) {
  uint8_t sdu[160];
  mini_gnb_c_buffer_t first_segment;
  mini_gnb_c_buffer_t second_segment;
  mini_gnb_c_buffer_t reassembled;
  mini_gnb_c_rlc_lite_receiver_t receiver;
  size_t consumed = 0u;
  bool is_last = false;
  size_t second_offset = 0u;
  size_t payload_bytes = 0u;
  size_t i = 0u;

  for (i = 0u; i < sizeof(sdu); ++i) {
    sdu[i] = (uint8_t)(0x80u + (i & 0x3fu));
  }

  mini_gnb_c_require(mini_gnb_c_rlc_lite_build_segment(11u,
                                                       sdu,
                                                       sizeof(sdu),
                                                       0u,
                                                       96u,
                                                       &first_segment,
                                                       &payload_bytes,
                                                       &is_last) == 0,
                     "expected first segment");
  second_offset = payload_bytes;
  mini_gnb_c_require(mini_gnb_c_rlc_lite_build_segment(11u,
                                                       sdu,
                                                       sizeof(sdu),
                                                       second_offset,
                                                       96u,
                                                       &second_segment,
                                                       &payload_bytes,
                                                       &is_last) == 0,
                     "expected second segment");

  mini_gnb_c_rlc_lite_receiver_init(&receiver);
  mini_gnb_c_require(mini_gnb_c_rlc_lite_receiver_consume(&receiver,
                                                          second_segment.bytes,
                                                          second_segment.len,
                                                          &reassembled,
                                                          &consumed) < 0,
                     "expected out-of-order segment rejection");
}
