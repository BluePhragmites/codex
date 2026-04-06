#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "test_helpers.h"

#include "mini_gnb_c/core/core_session.h"
#include "mini_gnb_c/n3/gtpu_tunnel.h"
#include "mini_gnb_c/n3/n3_user_plane.h"

static int mini_gnb_c_bind_udp_loopback(uint16_t* out_port) {
  struct sockaddr_in addr;
  socklen_t addr_len = sizeof(addr);
  int socket_fd = -1;

  mini_gnb_c_require(out_port != NULL, "expected output port");
  socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
  mini_gnb_c_require(socket_fd >= 0, "expected UDP socket");

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(0u);
  mini_gnb_c_require(bind(socket_fd, (const struct sockaddr*)&addr, sizeof(addr)) == 0, "expected UDP bind");
  mini_gnb_c_require(getsockname(socket_fd, (struct sockaddr*)&addr, &addr_len) == 0, "expected getsockname");
  *out_port = ntohs(addr.sin_port);
  return socket_fd;
}

static void mini_gnb_c_seed_user_plane_session(mini_gnb_c_core_session_t* session) {
  static const uint8_t k_ue_ipv4[4] = {10u, 45u, 0u, 7u};

  mini_gnb_c_require(session != NULL, "expected session");
  mini_gnb_c_core_session_reset(session);
  mini_gnb_c_require(mini_gnb_c_core_session_set_upf_tunnel(session, "127.0.0.1", 0x11223344u) == 0,
                     "expected UPF tunnel");
  mini_gnb_c_require(mini_gnb_c_core_session_set_qfi(session, 9u) == 0, "expected QFI");
  mini_gnb_c_core_session_set_ue_ipv4(session, k_ue_ipv4);
}

void test_n3_user_plane_activates_and_sends_uplink_gpdu(void) {
  static const uint8_t k_inner_packet[] = {0x45u, 0x00u, 0x00u, 0x1cu, 0xaau, 0xbbu};
  uint8_t received[MINI_GNB_C_N3_MAX_GTPU_PACKET];
  mini_gnb_c_core_session_t session;
  mini_gnb_c_n3_user_plane_t user_plane;
  uint16_t upf_port = 0u;
  uint16_t local_port = 0u;
  ssize_t received_length = 0;
  int upf_socket_fd = -1;

  upf_socket_fd = mini_gnb_c_bind_udp_loopback(&upf_port);
  mini_gnb_c_seed_user_plane_session(&session);
  mini_gnb_c_n3_user_plane_init(&user_plane);

  mini_gnb_c_require(mini_gnb_c_n3_user_plane_activate(&user_plane, &session, upf_port, 19) == 0,
                     "expected N3 user-plane activation");
  mini_gnb_c_require(mini_gnb_c_n3_user_plane_is_ready(&user_plane), "expected N3 user-plane ready");
  mini_gnb_c_require(user_plane.activation_count == 1u, "expected one activation");
  mini_gnb_c_require(user_plane.last_activation_abs_slot == 19, "expected activation slot");
  mini_gnb_c_require(mini_gnb_c_n3_user_plane_get_local_endpoint(&user_plane, NULL, 0u, &local_port) == 0,
                     "expected local endpoint query");
  mini_gnb_c_require(local_port != 0u, "expected ephemeral local UDP port");

  mini_gnb_c_require(mini_gnb_c_n3_user_plane_send_uplink(&user_plane, k_inner_packet, sizeof(k_inner_packet)) == 0,
                     "expected uplink G-PDU send");
  received_length = recv(upf_socket_fd, received, sizeof(received), 0);
  mini_gnb_c_require(received_length > 16, "expected G-PDU packet on UPF socket");
  mini_gnb_c_require(received[0] == 0x34u && received[1] == 0xffu, "expected GTP-U G-PDU header");
  mini_gnb_c_require(received[4] == 0x11u && received[5] == 0x22u && received[6] == 0x33u && received[7] == 0x44u,
                     "expected encoded TEID");
  mini_gnb_c_require(received[14] == 9u, "expected encoded QFI");
  mini_gnb_c_require(memcmp(received + 16, k_inner_packet, sizeof(k_inner_packet)) == 0,
                     "expected carried inner payload");
  mini_gnb_c_require(user_plane.uplink_gpdu_count == 1u, "expected uplink G-PDU counter");
  mini_gnb_c_require(user_plane.last_uplink_packet_length == (size_t)received_length,
                     "expected tracked uplink packet length");

  mini_gnb_c_n3_user_plane_close(&user_plane);
  close(upf_socket_fd);
}

void test_n3_user_plane_polls_downlink_packet(void) {
  static const uint8_t k_inner_packet[] = {0x45u, 0x00u, 0x00u, 0x20u, 0x01u, 0x02u, 0x03u, 0x04u};
  uint8_t packet[MINI_GNB_C_N3_MAX_GTPU_PACKET];
  uint8_t received[MINI_GNB_C_N3_MAX_GTPU_PACKET];
  mini_gnb_c_core_session_t session;
  mini_gnb_c_n3_user_plane_t user_plane;
  struct sockaddr_in dst_addr;
  size_t packet_length = 0u;
  size_t received_length = 0u;
  uint16_t upf_port = 0u;
  uint16_t local_port = 0u;
  int upf_socket_fd = -1;
  int poll_result = 0;
  int attempt = 0;

  upf_socket_fd = mini_gnb_c_bind_udp_loopback(&upf_port);
  mini_gnb_c_seed_user_plane_session(&session);
  mini_gnb_c_n3_user_plane_init(&user_plane);
  mini_gnb_c_require(mini_gnb_c_n3_user_plane_activate(&user_plane, &session, upf_port, 21) == 0,
                     "expected N3 user-plane activation");
  mini_gnb_c_require(mini_gnb_c_n3_user_plane_get_local_endpoint(&user_plane, NULL, 0u, &local_port) == 0,
                     "expected local endpoint query");

  memset(&dst_addr, 0, sizeof(dst_addr));
  dst_addr.sin_family = AF_INET;
  dst_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  dst_addr.sin_port = htons(local_port);
  mini_gnb_c_require(mini_gnb_c_gtpu_build_gpdu(&session,
                                                k_inner_packet,
                                                sizeof(k_inner_packet),
                                                packet,
                                                sizeof(packet),
                                                &packet_length) == 0,
                     "expected G-PDU build");
  mini_gnb_c_require(sendto(upf_socket_fd,
                            packet,
                            packet_length,
                            0,
                            (const struct sockaddr*)&dst_addr,
                            sizeof(dst_addr)) == (ssize_t)packet_length,
                     "expected downlink G-PDU send");

  for (attempt = 0; attempt < 20; ++attempt) {
    poll_result = mini_gnb_c_n3_user_plane_poll_downlink(&user_plane, received, sizeof(received), &received_length);
    if (poll_result == 1) {
      break;
    }
  }
  mini_gnb_c_require(poll_result == 1, "expected one downlink G-PDU");
  mini_gnb_c_require(received_length == packet_length, "expected tracked downlink packet length");
  mini_gnb_c_require(memcmp(received, packet, packet_length) == 0, "expected exact downlink G-PDU bytes");
  mini_gnb_c_require(user_plane.downlink_gpdu_count == 1u, "expected downlink G-PDU counter");
  mini_gnb_c_require(user_plane.last_downlink_packet_length == packet_length, "expected stored downlink length");

  mini_gnb_c_n3_user_plane_close(&user_plane);
  close(upf_socket_fd);
}
