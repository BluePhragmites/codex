#include "mini_gnb_c/n3/n3_user_plane.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "mini_gnb_c/n3/gtpu_tunnel.h"

static void mini_gnb_c_n3_user_plane_trace_packet(mini_gnb_c_n3_user_plane_t* user_plane,
                                                  const char* src_ip,
                                                  uint16_t src_port,
                                                  const char* dst_ip,
                                                  uint16_t dst_port,
                                                  const uint8_t* packet,
                                                  size_t packet_length) {
  if (user_plane == NULL || src_ip == NULL || dst_ip == NULL || packet == NULL || packet_length == 0u ||
      !mini_gnb_c_pcap_writer_is_open(&user_plane->gtpu_trace_writer)) {
    return;
  }
  (void)mini_gnb_c_pcap_writer_write_udp_ipv4(&user_plane->gtpu_trace_writer,
                                              src_ip,
                                              src_port,
                                              dst_ip,
                                              dst_port,
                                              packet,
                                              packet_length);
}

static int mini_gnb_c_n3_user_plane_open_socket(mini_gnb_c_n3_user_plane_t* user_plane) {
  struct sockaddr_in bind_addr;
  int socket_fd = -1;
  int flags = 0;

  if (user_plane == NULL) {
    return -1;
  }
  if (user_plane->socket_fd >= 0) {
    return 0;
  }

  socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (socket_fd < 0) {
    return -1;
  }

  memset(&bind_addr, 0, sizeof(bind_addr));
  bind_addr.sin_family = AF_INET;
  bind_addr.sin_port = htons(user_plane->local_port);
  if (user_plane->local_ip[0] != '\0') {
    if (inet_pton(AF_INET, user_plane->local_ip, &bind_addr.sin_addr) != 1) {
      close(socket_fd);
      return -1;
    }
  } else {
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  }
  if (bind(socket_fd, (const struct sockaddr*)&bind_addr, sizeof(bind_addr)) != 0) {
    close(socket_fd);
    return -1;
  }

  flags = fcntl(socket_fd, F_GETFL, 0);
  if (flags < 0 || fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) != 0) {
    close(socket_fd);
    return -1;
  }

  user_plane->socket_fd = socket_fd;
  return 0;
}

static int mini_gnb_c_n3_user_plane_connect(mini_gnb_c_n3_user_plane_t* user_plane) {
  struct sockaddr_in upf_addr;

  if (user_plane == NULL || user_plane->socket_fd < 0 || user_plane->upf_ip[0] == '\0' || user_plane->upf_port == 0u) {
    return -1;
  }

  memset(&upf_addr, 0, sizeof(upf_addr));
  upf_addr.sin_family = AF_INET;
  upf_addr.sin_port = htons(user_plane->upf_port);
  if (inet_pton(AF_INET, user_plane->upf_ip, &upf_addr.sin_addr) != 1) {
    return -1;
  }
  if (connect(user_plane->socket_fd, (const struct sockaddr*)&upf_addr, sizeof(upf_addr)) != 0) {
    return -1;
  }
  return 0;
}

int mini_gnb_c_n3_user_plane_resolve_local_ipv4(const char* upf_ip,
                                                uint16_t upf_port,
                                                char* local_ip,
                                                size_t local_ip_size) {
  struct sockaddr_in upf_addr;
  struct sockaddr_in local_addr;
  socklen_t local_addr_length = sizeof(local_addr);
  int socket_fd = -1;
  int result = -1;

  if (upf_ip == NULL || upf_ip[0] == '\0' || upf_port == 0u || local_ip == NULL || local_ip_size == 0u) {
    return -1;
  }

  socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (socket_fd < 0) {
    return -1;
  }

  memset(&upf_addr, 0, sizeof(upf_addr));
  upf_addr.sin_family = AF_INET;
  upf_addr.sin_port = htons(upf_port);
  if (inet_pton(AF_INET, upf_ip, &upf_addr.sin_addr) != 1) {
    close(socket_fd);
    return -1;
  }
  if (connect(socket_fd, (const struct sockaddr*)&upf_addr, sizeof(upf_addr)) != 0) {
    close(socket_fd);
    return -1;
  }
  if (getsockname(socket_fd, (struct sockaddr*)&local_addr, &local_addr_length) == 0 &&
      inet_ntop(AF_INET, &local_addr.sin_addr, local_ip, (socklen_t)local_ip_size) != NULL) {
    result = 0;
  }

  close(socket_fd);
  return result;
}

void mini_gnb_c_n3_user_plane_init(mini_gnb_c_n3_user_plane_t* user_plane) {
  if (user_plane == NULL) {
    return;
  }

  memset(user_plane, 0, sizeof(*user_plane));
  user_plane->socket_fd = -1;
  user_plane->last_activation_abs_slot = -1;
  mini_gnb_c_pcap_writer_init(&user_plane->gtpu_trace_writer);
}

int mini_gnb_c_n3_user_plane_set_gtpu_trace_path(mini_gnb_c_n3_user_plane_t* user_plane, const char* path) {
  if (user_plane == NULL) {
    return -1;
  }
  if (path == NULL || path[0] == '\0') {
    mini_gnb_c_pcap_writer_close(&user_plane->gtpu_trace_writer);
    user_plane->gtpu_trace_writer.path[0] = '\0';
    return 0;
  }
  return mini_gnb_c_pcap_writer_open(&user_plane->gtpu_trace_writer, path, MINI_GNB_C_PCAP_LINKTYPE_RAW);
}

const char* mini_gnb_c_n3_user_plane_get_gtpu_trace_path(const mini_gnb_c_n3_user_plane_t* user_plane) {
  return user_plane != NULL ? mini_gnb_c_pcap_writer_path(&user_plane->gtpu_trace_writer) : "";
}

void mini_gnb_c_n3_user_plane_close(mini_gnb_c_n3_user_plane_t* user_plane) {
  if (user_plane == NULL) {
    return;
  }

  if (user_plane->socket_fd >= 0) {
    close(user_plane->socket_fd);
    user_plane->socket_fd = -1;
  }
  user_plane->ready = false;
  mini_gnb_c_pcap_writer_close(&user_plane->gtpu_trace_writer);
}

int mini_gnb_c_n3_user_plane_activate(mini_gnb_c_n3_user_plane_t* user_plane,
                                      const mini_gnb_c_core_session_t* session,
                                      const uint16_t upf_port,
                                      const int abs_slot) {
  if (user_plane == NULL || session == NULL || upf_port == 0u || !mini_gnb_c_core_session_has_user_plane(session) ||
      session->upf_ip[0] == '\0') {
    return -1;
  }

  user_plane->session = *session;
  user_plane->upf_port = upf_port;
  user_plane->local_port = MINI_GNB_C_N3_GTPU_PORT;
  user_plane->downlink_teid = MINI_GNB_C_N3_DOWNLINK_TEID;
  (void)snprintf(user_plane->upf_ip, sizeof(user_plane->upf_ip), "%s", session->upf_ip);
  if (mini_gnb_c_n3_user_plane_resolve_local_ipv4(user_plane->upf_ip,
                                                  user_plane->upf_port,
                                                  user_plane->local_ip,
                                                  sizeof(user_plane->local_ip)) != 0) {
    return -1;
  }

  if (mini_gnb_c_n3_user_plane_open_socket(user_plane) != 0) {
    return -1;
  }
  if (mini_gnb_c_n3_user_plane_connect(user_plane) != 0) {
    return -1;
  }

  user_plane->ready = true;
  user_plane->last_activation_abs_slot = abs_slot;
  ++user_plane->activation_count;
  return 0;
}

bool mini_gnb_c_n3_user_plane_is_ready(const mini_gnb_c_n3_user_plane_t* user_plane) {
  return user_plane != NULL && user_plane->ready && user_plane->socket_fd >= 0;
}

int mini_gnb_c_n3_user_plane_get_local_endpoint(const mini_gnb_c_n3_user_plane_t* user_plane,
                                                char* ip_text,
                                                const size_t ip_text_size,
                                                uint16_t* port) {
  struct sockaddr_in local_addr;
  socklen_t local_addr_length = sizeof(local_addr);

  if (user_plane == NULL || user_plane->socket_fd < 0) {
    return -1;
  }
  if (getsockname(user_plane->socket_fd, (struct sockaddr*)&local_addr, &local_addr_length) != 0) {
    return -1;
  }
  if (port != NULL) {
    *port = ntohs(local_addr.sin_port);
  }
  if (ip_text != NULL && ip_text_size > 0u) {
    if (inet_ntop(AF_INET, &local_addr.sin_addr, ip_text, (socklen_t)ip_text_size) == NULL) {
      return -1;
    }
  }
  return 0;
}

int mini_gnb_c_n3_user_plane_send_uplink(mini_gnb_c_n3_user_plane_t* user_plane,
                                         const uint8_t* inner_packet,
                                         const size_t inner_packet_length) {
  uint8_t packet[MINI_GNB_C_N3_MAX_GTPU_PACKET];
  size_t packet_length = 0u;
  char local_ip[MINI_GNB_C_CORE_MAX_IPV4_TEXT];
  uint16_t local_port = 0u;
  ssize_t sent = 0;

  if (!mini_gnb_c_n3_user_plane_is_ready(user_plane) || inner_packet == NULL || inner_packet_length == 0u) {
    return -1;
  }
  if (mini_gnb_c_gtpu_build_gpdu(&user_plane->session,
                                 inner_packet,
                                 inner_packet_length,
                                 packet,
                                 sizeof(packet),
                                 &packet_length) != 0) {
    return -1;
  }

  sent = send(user_plane->socket_fd, packet, packet_length, 0);
  if (sent != (ssize_t)packet_length) {
    return -1;
  }
  if (mini_gnb_c_n3_user_plane_get_local_endpoint(user_plane, local_ip, sizeof(local_ip), &local_port) == 0) {
    mini_gnb_c_n3_user_plane_trace_packet(user_plane,
                                          local_ip,
                                          local_port,
                                          user_plane->upf_ip,
                                          user_plane->upf_port,
                                          packet,
                                          packet_length);
  }

  user_plane->last_uplink_packet_length = packet_length;
  ++user_plane->uplink_gpdu_count;
  return 0;
}

int mini_gnb_c_n3_user_plane_poll_downlink(mini_gnb_c_n3_user_plane_t* user_plane,
                                           uint8_t* packet,
                                           const size_t packet_capacity,
                                           size_t* packet_length) {
  char local_ip[MINI_GNB_C_CORE_MAX_IPV4_TEXT];
  uint16_t local_port = 0u;
  ssize_t received = 0;

  if (packet_length != NULL) {
    *packet_length = 0u;
  }
  if (!mini_gnb_c_n3_user_plane_is_ready(user_plane) || packet == NULL || packet_capacity == 0u) {
    return -1;
  }

  received = recv(user_plane->socket_fd, packet, packet_capacity, 0);
  if (received < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return 0;
    }
    return -1;
  }

  if (packet_length != NULL) {
    *packet_length = (size_t)received;
  }
  if (mini_gnb_c_n3_user_plane_get_local_endpoint(user_plane, local_ip, sizeof(local_ip), &local_port) == 0) {
    mini_gnb_c_n3_user_plane_trace_packet(user_plane,
                                          user_plane->upf_ip,
                                          user_plane->upf_port,
                                          local_ip,
                                          local_port,
                                          packet,
                                          (size_t)received);
  }
  user_plane->last_downlink_packet_length = (size_t)received;
  ++user_plane->downlink_gpdu_count;
  return 1;
}
