#ifndef MINI_GNB_C_UE_UE_TUN_H
#define MINI_GNB_C_UE_UE_TUN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mini_gnb_c/common/types.h"

typedef struct {
  int fd;
  bool opened;
  bool configured;
  bool isolate_netns;
  char ifname[MINI_GNB_C_MAX_TEXT];
  uint16_t mtu;
  uint8_t prefix_len;
  uint8_t local_ipv4[4];
} mini_gnb_c_ue_tun_t;

void mini_gnb_c_ue_tun_init(mini_gnb_c_ue_tun_t* tun);
int mini_gnb_c_ue_tun_open(mini_gnb_c_ue_tun_t* tun, const mini_gnb_c_sim_config_t* sim);
int mini_gnb_c_ue_tun_configure_ipv4(mini_gnb_c_ue_tun_t* tun, const uint8_t ue_ipv4[4]);
int mini_gnb_c_ue_tun_read_packet(mini_gnb_c_ue_tun_t* tun, uint8_t* out_packet, size_t out_size, size_t* out_length);
int mini_gnb_c_ue_tun_write_packet(mini_gnb_c_ue_tun_t* tun, const uint8_t* packet, size_t packet_length);
void mini_gnb_c_ue_tun_close(mini_gnb_c_ue_tun_t* tun);

#endif
