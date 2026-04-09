#ifndef MINI_GNB_C_N3_N3_USER_PLANE_H
#define MINI_GNB_C_N3_N3_USER_PLANE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mini_gnb_c/core/core_session.h"
#include "mini_gnb_c/trace/pcap_trace.h"

#define MINI_GNB_C_N3_MAX_GTPU_PACKET 4096u
#define MINI_GNB_C_N3_GTPU_PORT 2152u
#define MINI_GNB_C_N3_DOWNLINK_TEID 0x00000001u

typedef struct {
  int socket_fd;
  bool ready;
  mini_gnb_c_core_session_t session;
  char upf_ip[MINI_GNB_C_CORE_MAX_IPV4_TEXT];
  char local_ip[MINI_GNB_C_CORE_MAX_IPV4_TEXT];
  uint16_t upf_port;
  uint16_t local_port;
  uint32_t downlink_teid;
  uint64_t activation_count;
  uint64_t uplink_gpdu_count;
  uint64_t downlink_gpdu_count;
  size_t last_uplink_packet_length;
  size_t last_downlink_packet_length;
  int last_activation_abs_slot;
  mini_gnb_c_pcap_writer_t gtpu_trace_writer;
} mini_gnb_c_n3_user_plane_t;

void mini_gnb_c_n3_user_plane_init(mini_gnb_c_n3_user_plane_t* user_plane);
int mini_gnb_c_n3_user_plane_set_gtpu_trace_path(mini_gnb_c_n3_user_plane_t* user_plane, const char* path);
const char* mini_gnb_c_n3_user_plane_get_gtpu_trace_path(const mini_gnb_c_n3_user_plane_t* user_plane);
void mini_gnb_c_n3_user_plane_close(mini_gnb_c_n3_user_plane_t* user_plane);
int mini_gnb_c_n3_user_plane_resolve_local_ipv4(const char* upf_ip,
                                                uint16_t upf_port,
                                                char* local_ip,
                                                size_t local_ip_size);
int mini_gnb_c_n3_user_plane_activate(mini_gnb_c_n3_user_plane_t* user_plane,
                                      const mini_gnb_c_core_session_t* session,
                                      uint16_t upf_port,
                                      int abs_slot);
bool mini_gnb_c_n3_user_plane_is_ready(const mini_gnb_c_n3_user_plane_t* user_plane);
int mini_gnb_c_n3_user_plane_get_local_endpoint(const mini_gnb_c_n3_user_plane_t* user_plane,
                                                char* ip_text,
                                                size_t ip_text_size,
                                                uint16_t* port);
int mini_gnb_c_n3_user_plane_send_uplink(mini_gnb_c_n3_user_plane_t* user_plane,
                                         const uint8_t* inner_packet,
                                         size_t inner_packet_length);
int mini_gnb_c_n3_user_plane_poll_downlink(mini_gnb_c_n3_user_plane_t* user_plane,
                                           uint8_t* packet,
                                           size_t packet_capacity,
                                           size_t* packet_length);

#endif
