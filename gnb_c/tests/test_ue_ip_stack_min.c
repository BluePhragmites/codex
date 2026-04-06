#include <string.h>

#include "test_helpers.h"

#include "mini_gnb_c/ue/ue_ip_stack_min.h"

static uint16_t mini_gnb_c_test_checksum16(const uint8_t* data, const size_t length) {
  uint32_t sum = 0u;
  size_t index = 0u;

  while (index + 1u < length) {
    sum += (uint32_t)(((uint16_t)data[index] << 8u) | data[index + 1u]);
    index += 2u;
  }
  if (index < length) {
    sum += (uint32_t)((uint16_t)data[index] << 8u);
  }
  while ((sum >> 16u) != 0u) {
    sum = (sum & 0xffffu) + (sum >> 16u);
  }
  return (uint16_t)(~sum & 0xffffu);
}

static void mini_gnb_c_build_icmp_echo_request(mini_gnb_c_buffer_t* packet) {
  static const uint8_t k_src_ipv4[4] = {10u, 45u, 0u, 1u};
  static const uint8_t k_dst_ipv4[4] = {10u, 45u, 0u, 7u};
  static const uint8_t k_payload[] = {'p', 'i', 'n', 'g'};
  const uint16_t total_length = (uint16_t)(20u + 8u + sizeof(k_payload));
  const uint16_t identifier = 0x1234u;
  const uint16_t sequence = 0x0001u;
  uint16_t checksum = 0u;

  mini_gnb_c_require(packet != NULL, "expected packet for ICMP echo request");
  mini_gnb_c_buffer_reset(packet);
  mini_gnb_c_require(total_length <= sizeof(packet->bytes), "expected ICMP echo request buffer capacity");
  packet->len = total_length;
  memset(packet->bytes, 0, packet->len);
  packet->bytes[0] = 0x45u;
  packet->bytes[2] = (uint8_t)(total_length >> 8u);
  packet->bytes[3] = (uint8_t)(total_length & 0xffu);
  packet->bytes[4] = 0x11u;
  packet->bytes[5] = 0x22u;
  packet->bytes[8] = 64u;
  packet->bytes[9] = 1u;
  memcpy(packet->bytes + 12u, k_src_ipv4, sizeof(k_src_ipv4));
  memcpy(packet->bytes + 16u, k_dst_ipv4, sizeof(k_dst_ipv4));
  checksum = mini_gnb_c_test_checksum16(packet->bytes, 20u);
  packet->bytes[10] = (uint8_t)(checksum >> 8u);
  packet->bytes[11] = (uint8_t)(checksum & 0xffu);

  packet->bytes[20] = 8u;
  packet->bytes[21] = 0u;
  packet->bytes[24] = (uint8_t)(identifier >> 8u);
  packet->bytes[25] = (uint8_t)(identifier & 0xffu);
  packet->bytes[26] = (uint8_t)(sequence >> 8u);
  packet->bytes[27] = (uint8_t)(sequence & 0xffu);
  memcpy(packet->bytes + 28u, k_payload, sizeof(k_payload));
  checksum = mini_gnb_c_test_checksum16(packet->bytes + 20u, 8u + sizeof(k_payload));
  packet->bytes[22] = (uint8_t)(checksum >> 8u);
  packet->bytes[23] = (uint8_t)(checksum & 0xffu);
}

void test_ue_ip_stack_min_generates_echo_reply(void) {
  mini_gnb_c_ue_ip_stack_min_t stack;
  mini_gnb_c_buffer_t request;
  mini_gnb_c_buffer_t reply;

  mini_gnb_c_build_icmp_echo_request(&request);
  mini_gnb_c_ue_ip_stack_min_init(&stack);

  mini_gnb_c_require(mini_gnb_c_ue_ip_stack_min_handle_downlink(&stack, &request) == 1,
                     "expected ICMP echo request handling");
  mini_gnb_c_require(stack.downlink_echo_request_count == 1u, "expected one downlink echo request");
  mini_gnb_c_require(mini_gnb_c_ue_ip_stack_min_has_pending_uplink(&stack), "expected pending echo reply");
  mini_gnb_c_require(mini_gnb_c_ue_ip_stack_min_copy_pending_uplink(&stack, &reply) == 0,
                     "expected pending reply copy");
  mini_gnb_c_require(reply.len == request.len, "expected echoed total length");
  mini_gnb_c_require(reply.bytes[20] == 0u && reply.bytes[21] == 0u, "expected ICMP echo reply type/code");
  mini_gnb_c_require(memcmp(reply.bytes + 12u, request.bytes + 16u, 4u) == 0, "expected swapped source IPv4");
  mini_gnb_c_require(memcmp(reply.bytes + 16u, request.bytes + 12u, 4u) == 0,
                     "expected swapped destination IPv4");
  mini_gnb_c_require(memcmp(reply.bytes + 24u, request.bytes + 24u, 4u) == 0,
                     "expected preserved identifier and sequence");
  mini_gnb_c_require(memcmp(reply.bytes + 28u, request.bytes + 28u, 4u) == 0, "expected preserved payload bytes");

  mini_gnb_c_ue_ip_stack_min_mark_uplink_consumed(&stack);
  mini_gnb_c_require(!mini_gnb_c_ue_ip_stack_min_has_pending_uplink(&stack), "expected consumed reply");
  mini_gnb_c_require(stack.uplink_echo_reply_count == 1u, "expected one uplink echo reply");
}

void test_ue_ip_stack_min_ignores_non_ipv4_payload(void) {
  mini_gnb_c_ue_ip_stack_min_t stack;
  mini_gnb_c_buffer_t packet;

  mini_gnb_c_ue_ip_stack_min_init(&stack);
  mini_gnb_c_require(mini_gnb_c_buffer_set_text(&packet, "PUCCH_CFG|sr_abs_slot=12") == 0,
                     "expected control payload text");
  mini_gnb_c_require(mini_gnb_c_ue_ip_stack_min_handle_downlink(&stack, &packet) == 0,
                     "expected non-IPv4 payload ignore");
  mini_gnb_c_require(!mini_gnb_c_ue_ip_stack_min_has_pending_uplink(&stack), "expected no pending reply");
}
