#ifndef MINI_GNB_C_N3_GTPU_TUNNEL_H
#define MINI_GNB_C_N3_GTPU_TUNNEL_H

#include <stddef.h>
#include <stdint.h>

#include "mini_gnb_c/core/core_session.h"

int mini_gnb_c_gtpu_build_echo_request(uint16_t sequence_number,
                                       uint8_t* packet,
                                       size_t packet_capacity,
                                       size_t* packet_length);
int mini_gnb_c_gtpu_validate_echo_response(const uint8_t* packet, size_t packet_length, uint16_t sequence_number);
int mini_gnb_c_gtpu_build_ipv4_udp_probe(const mini_gnb_c_core_session_t* session,
                                         const char* dst_ipv4,
                                         uint8_t* packet,
                                         size_t packet_capacity,
                                         size_t* packet_length);
int mini_gnb_c_gtpu_build_gpdu(const mini_gnb_c_core_session_t* session,
                               const uint8_t* inner_packet,
                               size_t inner_packet_length,
                               uint8_t* packet,
                               size_t packet_capacity,
                               size_t* packet_length);

#endif
