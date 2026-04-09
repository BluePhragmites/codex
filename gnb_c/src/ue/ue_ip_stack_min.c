#include "mini_gnb_c/ue/ue_ip_stack_min.h"

#include <string.h>

static uint16_t mini_gnb_c_ue_ip_checksum16(const uint8_t* data, const size_t length) {
  uint32_t sum = 0u;
  size_t index = 0u;

  if (data == NULL) {
    return 0u;
  }

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

static size_t mini_gnb_c_ue_ipv4_header_length(const uint8_t* packet, const size_t packet_length) {
  const size_t ihl_bytes = (size_t)(packet[0] & 0x0fu) * 4u;

  if (packet == NULL || packet_length < 20u || (packet[0] >> 4u) != 4u || ihl_bytes < 20u || ihl_bytes > packet_length) {
    return 0u;
  }
  return ihl_bytes;
}

static size_t mini_gnb_c_ue_ipv4_total_length(const uint8_t* packet, const size_t packet_length) {
  const size_t total_length = ((size_t)packet[2] << 8u) | (size_t)packet[3];
  const size_t header_length = mini_gnb_c_ue_ipv4_header_length(packet, packet_length);

  if (header_length == 0u || total_length < header_length || total_length > packet_length) {
    return 0u;
  }
  return total_length;
}

void mini_gnb_c_ue_ip_stack_min_init(mini_gnb_c_ue_ip_stack_min_t* stack) {
  if (stack == NULL) {
    return;
  }

  memset(stack, 0, sizeof(*stack));
}

bool mini_gnb_c_ue_ip_stack_min_is_ipv4_packet(const uint8_t* packet, const size_t packet_length) {
  return mini_gnb_c_ue_ipv4_total_length(packet, packet_length) > 0u;
}

int mini_gnb_c_ue_ip_stack_min_handle_downlink(mini_gnb_c_ue_ip_stack_min_t* stack, const mini_gnb_c_buffer_t* packet) {
  uint8_t reply[MINI_GNB_C_MAX_PAYLOAD];
  size_t header_length = 0u;
  size_t total_length = 0u;
  size_t icmp_length = 0u;
  uint8_t src_ipv4[4];
  uint8_t dst_ipv4[4];

  if (stack == NULL || packet == NULL || packet->len == 0u) {
    return -1;
  }

  header_length = mini_gnb_c_ue_ipv4_header_length(packet->bytes, packet->len);
  total_length = mini_gnb_c_ue_ipv4_total_length(packet->bytes, packet->len);
  if (header_length == 0u || total_length == 0u) {
    return 0;
  }
  if (packet->bytes[9] != 1u) {
    return 0;
  }
  if (total_length > sizeof(reply) || total_length < header_length + 8u) {
    return 0;
  }

  icmp_length = total_length - header_length;
  if (packet->bytes[header_length] != 8u || packet->bytes[header_length + 1u] != 0u) {
    return 0;
  }

  memcpy(src_ipv4, packet->bytes + 12u, sizeof(src_ipv4));
  memcpy(dst_ipv4, packet->bytes + 16u, sizeof(dst_ipv4));
  if (stack->local_ipv4_valid && memcmp(dst_ipv4, stack->local_ipv4, sizeof(dst_ipv4)) != 0) {
    return 0;
  }

  memcpy(reply, packet->bytes, total_length);
  memcpy(reply + 12u, dst_ipv4, sizeof(dst_ipv4));
  memcpy(reply + 16u, src_ipv4, sizeof(src_ipv4));
  reply[8] = 64u;
  reply[header_length] = 0u;
  reply[header_length + 2u] = 0u;
  reply[header_length + 3u] = 0u;
  {
    const uint16_t icmp_checksum = mini_gnb_c_ue_ip_checksum16(reply + header_length, icmp_length);
    reply[header_length + 2u] = (uint8_t)(icmp_checksum >> 8u);
    reply[header_length + 3u] = (uint8_t)(icmp_checksum & 0xffu);
  }
  reply[10] = 0u;
  reply[11] = 0u;
  {
    const uint16_t ip_checksum = mini_gnb_c_ue_ip_checksum16(reply, header_length);
    reply[10] = (uint8_t)(ip_checksum >> 8u);
    reply[11] = (uint8_t)(ip_checksum & 0xffu);
  }

  stack->local_ipv4_valid = true;
  memcpy(stack->local_ipv4, dst_ipv4, sizeof(dst_ipv4));
  if (mini_gnb_c_buffer_set_bytes(&stack->pending_uplink_packet, reply, total_length) != 0) {
    return -1;
  }
  stack->pending_uplink_valid = true;
  ++stack->downlink_echo_request_count;
  return 1;
}

bool mini_gnb_c_ue_ip_stack_min_has_pending_uplink(const mini_gnb_c_ue_ip_stack_min_t* stack) {
  return stack != NULL && stack->pending_uplink_valid && stack->pending_uplink_packet.len > 0u;
}

int mini_gnb_c_ue_ip_stack_min_copy_pending_uplink(const mini_gnb_c_ue_ip_stack_min_t* stack,
                                                   mini_gnb_c_buffer_t* out_packet) {
  if (!mini_gnb_c_ue_ip_stack_min_has_pending_uplink(stack) || out_packet == NULL) {
    return -1;
  }

  *out_packet = stack->pending_uplink_packet;
  return 0;
}

void mini_gnb_c_ue_ip_stack_min_mark_uplink_consumed(mini_gnb_c_ue_ip_stack_min_t* stack) {
  if (stack == NULL || !stack->pending_uplink_valid) {
    return;
  }

  stack->pending_uplink_valid = false;
  ++stack->uplink_echo_reply_count;
}
