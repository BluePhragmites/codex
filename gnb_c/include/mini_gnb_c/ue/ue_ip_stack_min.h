#ifndef MINI_GNB_C_UE_UE_IP_STACK_MIN_H
#define MINI_GNB_C_UE_UE_IP_STACK_MIN_H

#include <stdbool.h>
#include <stdint.h>

#include "mini_gnb_c/common/types.h"

typedef struct {
  bool pending_uplink_valid;
  bool local_ipv4_valid;
  uint8_t local_ipv4[4];
  mini_gnb_c_buffer_t pending_uplink_packet;
  uint64_t downlink_echo_request_count;
  uint64_t uplink_echo_reply_count;
} mini_gnb_c_ue_ip_stack_min_t;

void mini_gnb_c_ue_ip_stack_min_init(mini_gnb_c_ue_ip_stack_min_t* stack);
bool mini_gnb_c_ue_ip_stack_min_is_ipv4_packet(const uint8_t* packet, size_t packet_length);
int mini_gnb_c_ue_ip_stack_min_handle_downlink(mini_gnb_c_ue_ip_stack_min_t* stack,
                                               const mini_gnb_c_buffer_t* packet);
bool mini_gnb_c_ue_ip_stack_min_has_pending_uplink(const mini_gnb_c_ue_ip_stack_min_t* stack);
int mini_gnb_c_ue_ip_stack_min_copy_pending_uplink(const mini_gnb_c_ue_ip_stack_min_t* stack,
                                                   mini_gnb_c_buffer_t* out_packet);
void mini_gnb_c_ue_ip_stack_min_mark_uplink_consumed(mini_gnb_c_ue_ip_stack_min_t* stack);

#endif
