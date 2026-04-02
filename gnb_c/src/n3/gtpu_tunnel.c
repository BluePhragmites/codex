#include "mini_gnb_c/n3/gtpu_tunnel.h"

#include <arpa/inet.h>
#include <string.h>

static uint16_t mini_gnb_c_checksum16(const uint8_t* data, const size_t length) {
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

int mini_gnb_c_gtpu_build_echo_request(const uint16_t sequence_number,
                                       uint8_t* packet,
                                       const size_t packet_capacity,
                                       size_t* packet_length) {
  if (packet == NULL || packet_length == NULL || packet_capacity < 14u) {
    return -1;
  }

  memset(packet, 0, 14u);
  packet[0] = 0x32u; /* GTPv1-U, PT=1, S=1 */
  packet[1] = 0x01u; /* Echo Request */
  packet[2] = 0x00u;
  packet[3] = 0x06u;
  packet[8] = (uint8_t)(sequence_number >> 8u);
  packet[9] = (uint8_t)(sequence_number & 0xffu);
  packet[12] = 0x0eu; /* Recovery IE */
  packet[13] = 0x00u;
  *packet_length = 14u;
  return 0;
}

int mini_gnb_c_gtpu_validate_echo_response(const uint8_t* packet,
                                           const size_t packet_length,
                                           const uint16_t sequence_number) {
  if (packet == NULL || packet_length < 14u) {
    return -1;
  }
  if (packet[0] != 0x32u || packet[1] != 0x02u) {
    return -1;
  }
  if (packet[8] != (uint8_t)(sequence_number >> 8u) || packet[9] != (uint8_t)(sequence_number & 0xffu)) {
    return -1;
  }

  return 0;
}

int mini_gnb_c_gtpu_build_ipv4_udp_probe(const mini_gnb_c_core_session_t* session,
                                         const char* dst_ipv4,
                                         uint8_t* packet,
                                         const size_t packet_capacity,
                                         size_t* packet_length) {
  static const uint8_t k_udp_payload[] = "mini_gnb_c_gpdu";
  struct in_addr dst_addr;
  uint16_t total_length = 0u;
  uint16_t udp_length = 0u;
  uint16_t ip_checksum = 0u;

  if (session == NULL || dst_ipv4 == NULL || packet == NULL || packet_length == NULL || !session->ue_ipv4_valid) {
    return -1;
  }
  if (inet_pton(AF_INET, dst_ipv4, &dst_addr) != 1) {
    return -1;
  }

  total_length = (uint16_t)(20u + 8u + sizeof(k_udp_payload) - 1u);
  udp_length = (uint16_t)(8u + sizeof(k_udp_payload) - 1u);
  if ((size_t)total_length > packet_capacity) {
    return -1;
  }

  memset(packet, 0, total_length);
  packet[0] = 0x45u;
  packet[1] = 0x00u;
  packet[2] = (uint8_t)(total_length >> 8u);
  packet[3] = (uint8_t)(total_length & 0xffu);
  packet[4] = 0x12u;
  packet[5] = 0x34u;
  packet[8] = 64u;
  packet[9] = 17u;
  memcpy(packet + 12u, session->ue_ipv4, 4u);
  memcpy(packet + 16u, &dst_addr.s_addr, 4u);
  ip_checksum = mini_gnb_c_checksum16(packet, 20u);
  packet[10] = (uint8_t)(ip_checksum >> 8u);
  packet[11] = (uint8_t)(ip_checksum & 0xffu);

  packet[20] = 0xd4u;
  packet[21] = 0x31u;
  packet[22] = 0x82u;
  packet[23] = 0x9au;
  packet[24] = (uint8_t)(udp_length >> 8u);
  packet[25] = (uint8_t)(udp_length & 0xffu);
  memcpy(packet + 28u, k_udp_payload, sizeof(k_udp_payload) - 1u);

  *packet_length = total_length;
  return 0;
}

int mini_gnb_c_gtpu_build_gpdu(const mini_gnb_c_core_session_t* session,
                               const uint8_t* inner_packet,
                               const size_t inner_packet_length,
                               uint8_t* packet,
                               const size_t packet_capacity,
                               size_t* packet_length) {
  uint16_t gtp_length = 0u;

  if (session == NULL || inner_packet == NULL || packet == NULL || packet_length == NULL || inner_packet_length == 0u) {
    return -1;
  }
  if (!mini_gnb_c_core_session_has_user_plane(session)) {
    return -1;
  }
  if (packet_capacity < 16u + inner_packet_length) {
    return -1;
  }

  memset(packet, 0, 16u + inner_packet_length);
  packet[0] = 0x34u; /* GTPv1-U, PT=1, E=1 */
  packet[1] = 0xffu; /* G-PDU */
  gtp_length = (uint16_t)(inner_packet_length + 8u);
  packet[2] = (uint8_t)(gtp_length >> 8u);
  packet[3] = (uint8_t)(gtp_length & 0xffu);
  packet[4] = (uint8_t)(session->upf_teid >> 24u);
  packet[5] = (uint8_t)((session->upf_teid >> 16u) & 0xffu);
  packet[6] = (uint8_t)((session->upf_teid >> 8u) & 0xffu);
  packet[7] = (uint8_t)(session->upf_teid & 0xffu);
  packet[11] = 0x85u; /* PDU Session Container */
  packet[12] = 0x01u; /* extension length */
  packet[13] = 0x10u; /* UL PDU Session Information */
  packet[14] = (uint8_t)(session->qfi & 0x3fu);
  memcpy(packet + 16u, inner_packet, inner_packet_length);

  *packet_length = 16u + inner_packet_length;
  return 0;
}
