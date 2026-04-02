#include "mini_gnb_c/ngap/ngap_transport.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/sctp.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#ifndef IPPROTO_SCTP
#define IPPROTO_SCTP 132
#endif

#ifndef MINI_GNB_C_NGAP_PPID
#define MINI_GNB_C_NGAP_PPID 60u
#endif

static int mini_gnb_c_ngap_transport_set_timeouts(const int socket_fd, const uint32_t timeout_ms) {
  struct timeval timeout;

  timeout.tv_sec = (time_t)(timeout_ms / 1000u);
  timeout.tv_usec = (suseconds_t)((timeout_ms % 1000u) * 1000u);

  if (setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, (socklen_t)sizeof(timeout)) != 0) {
    perror("setsockopt(SO_SNDTIMEO)");
    return -1;
  }
  if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, (socklen_t)sizeof(timeout)) != 0) {
    perror("setsockopt(SO_RCVTIMEO)");
    return -1;
  }
  return 0;
}

static int mini_gnb_c_ngap_transport_set_ppid(const int socket_fd) {
  struct sctp_sndrcvinfo sndrcv;

  memset(&sndrcv, 0, sizeof(sndrcv));
  sndrcv.sinfo_ppid = htonl(MINI_GNB_C_NGAP_PPID);

  if (setsockopt(socket_fd,
                 IPPROTO_SCTP,
                 SCTP_DEFAULT_SEND_PARAM,
                 &sndrcv,
                 (socklen_t)sizeof(sndrcv)) != 0) {
    perror("setsockopt(SCTP_DEFAULT_SEND_PARAM)");
    return -1;
  }
  return 0;
}

static int mini_gnb_c_ngap_transport_connect_with_timeout(const int socket_fd,
                                                          const struct sockaddr* address,
                                                          const socklen_t address_length,
                                                          const uint32_t timeout_ms) {
  int flags = 0;
  int connect_result = 0;
  struct pollfd poll_fd;
  uint32_t waited_ms = 0u;

  if (socket_fd < 0 || address == NULL) {
    return -1;
  }

  flags = fcntl(socket_fd, F_GETFL, 0);
  if (flags < 0) {
    perror("fcntl(F_GETFL)");
    return -1;
  }
  if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) != 0) {
    perror("fcntl(F_SETFL,O_NONBLOCK)");
    return -1;
  }

  connect_result = connect(socket_fd, address, address_length);
  if (connect_result == 0) {
    (void)fcntl(socket_fd, F_SETFL, flags);
    return 0;
  }
  if (errno != EINPROGRESS && errno != EALREADY && errno != EINTR) {
    perror("connect");
    (void)fcntl(socket_fd, F_SETFL, flags);
    return -1;
  }

  memset(&poll_fd, 0, sizeof(poll_fd));
  poll_fd.fd = socket_fd;
  poll_fd.events = POLLOUT;

  while (waited_ms < timeout_ms) {
    uint32_t step_ms = timeout_ms - waited_ms;
    int poll_result = 0;
    int socket_error = 0;
    socklen_t socket_error_length = (socklen_t)sizeof(socket_error);

    if (step_ms > 250u) {
      step_ms = 250u;
    }

    poll_result = poll(&poll_fd, 1, (int)step_ms);
    if (poll_result < 0) {
      if (errno == EINTR) {
        continue;
      }
      perror("poll");
      (void)fcntl(socket_fd, F_SETFL, flags);
      return -1;
    }
    waited_ms += step_ms;
    if (poll_result == 0) {
      continue;
    }

    if (getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &socket_error, &socket_error_length) != 0) {
      perror("getsockopt(SO_ERROR)");
      (void)fcntl(socket_fd, F_SETFL, flags);
      return -1;
    }
    if (socket_error == 0 || socket_error == EISCONN) {
      (void)fcntl(socket_fd, F_SETFL, flags);
      return 0;
    }
    if (socket_error == EINPROGRESS || socket_error == EALREADY) {
      continue;
    }

    errno = socket_error;
    perror("connect");
    (void)fcntl(socket_fd, F_SETFL, flags);
    return -1;
  }

  fprintf(stderr, "connect timed out after %u ms\n", timeout_ms);
  (void)fcntl(socket_fd, F_SETFL, flags);
  return -1;
}

static int mini_gnb_c_ngap_transport_default_connect(mini_gnb_c_ngap_transport_t* transport,
                                                     const char* amf_ip,
                                                     const uint32_t amf_port,
                                                     const uint32_t timeout_ms) {
  struct sockaddr_in amf_addr;
  int socket_fd = -1;

  if (transport == NULL || amf_ip == NULL || amf_ip[0] == '\0') {
    return -1;
  }
  if (transport->socket_fd >= 0) {
    return 0;
  }

  memset(&amf_addr, 0, sizeof(amf_addr));
  amf_addr.sin_family = AF_INET;
  amf_addr.sin_port = htons((uint16_t)amf_port);
  if (inet_pton(AF_INET, amf_ip, &amf_addr.sin_addr) != 1) {
    fprintf(stderr, "invalid amf_ip: %s\n", amf_ip);
    return -1;
  }

  socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
  if (socket_fd < 0) {
    perror("socket(SCTP)");
    return -1;
  }
  if (mini_gnb_c_ngap_transport_set_timeouts(socket_fd, timeout_ms) != 0 ||
      mini_gnb_c_ngap_transport_set_ppid(socket_fd) != 0 ||
      mini_gnb_c_ngap_transport_connect_with_timeout(socket_fd,
                                                     (const struct sockaddr*)&amf_addr,
                                                     (socklen_t)sizeof(amf_addr),
                                                     timeout_ms) != 0) {
    close(socket_fd);
    return -1;
  }

  transport->socket_fd = socket_fd;
  return 0;
}

static int mini_gnb_c_ngap_transport_default_send(mini_gnb_c_ngap_transport_t* transport,
                                                  const uint8_t* bytes,
                                                  const size_t length) {
  ssize_t bytes_sent = 0;
  int send_flags = 0;

  if (transport == NULL || transport->socket_fd < 0 || bytes == NULL || length == 0u) {
    return -1;
  }

#ifdef MSG_NOSIGNAL
  send_flags = MSG_NOSIGNAL;
#endif

  bytes_sent = send(transport->socket_fd, bytes, length, send_flags);
  if (bytes_sent < 0) {
    perror("send");
    return -1;
  }
  if ((size_t)bytes_sent != length) {
    fprintf(stderr, "short send: expected %zu, got %zd\n", length, bytes_sent);
    return -1;
  }
  return 0;
}

static int mini_gnb_c_ngap_transport_default_recv(mini_gnb_c_ngap_transport_t* transport,
                                                  uint8_t* response,
                                                  const size_t response_capacity,
                                                  size_t* response_length) {
  ssize_t bytes_received = 0;

  if (transport == NULL || transport->socket_fd < 0 || response == NULL || response_capacity == 0u ||
      response_length == NULL) {
    return -1;
  }

  bytes_received = recv(transport->socket_fd, response, response_capacity, 0);
  if (bytes_received < 0) {
    perror("recv");
    return -1;
  }
  *response_length = (size_t)bytes_received;
  return 0;
}

static void mini_gnb_c_ngap_transport_default_close(mini_gnb_c_ngap_transport_t* transport) {
  if (transport == NULL) {
    return;
  }
  if (transport->socket_fd >= 0) {
    close(transport->socket_fd);
    transport->socket_fd = -1;
  }
}

static const mini_gnb_c_ngap_transport_ops_t g_mini_gnb_c_ngap_transport_default_ops = {
    .connect = mini_gnb_c_ngap_transport_default_connect,
    .send = mini_gnb_c_ngap_transport_default_send,
    .recv = mini_gnb_c_ngap_transport_default_recv,
    .close = mini_gnb_c_ngap_transport_default_close,
};

void mini_gnb_c_ngap_transport_init(mini_gnb_c_ngap_transport_t* transport) {
  if (transport == NULL) {
    return;
  }
  memset(transport, 0, sizeof(*transport));
  transport->socket_fd = -1;
  transport->ops = &g_mini_gnb_c_ngap_transport_default_ops;
}

void mini_gnb_c_ngap_transport_set_ops(mini_gnb_c_ngap_transport_t* transport,
                                       const mini_gnb_c_ngap_transport_ops_t* ops,
                                       void* user_data) {
  if (transport == NULL) {
    return;
  }
  transport->ops = (ops != NULL) ? ops : &g_mini_gnb_c_ngap_transport_default_ops;
  transport->user_data = user_data;
  if (transport->socket_fd < 0) {
    transport->socket_fd = -1;
  }
}

int mini_gnb_c_ngap_transport_connect(mini_gnb_c_ngap_transport_t* transport,
                                      const char* amf_ip,
                                      const uint32_t amf_port,
                                      const uint32_t timeout_ms) {
  if (transport == NULL || transport->ops == NULL || transport->ops->connect == NULL) {
    return -1;
  }
  return transport->ops->connect(transport, amf_ip, amf_port, timeout_ms);
}

int mini_gnb_c_ngap_transport_send(mini_gnb_c_ngap_transport_t* transport,
                                   const uint8_t* bytes,
                                   const size_t length) {
  if (transport == NULL || transport->ops == NULL || transport->ops->send == NULL) {
    return -1;
  }
  return transport->ops->send(transport, bytes, length);
}

int mini_gnb_c_ngap_transport_recv(mini_gnb_c_ngap_transport_t* transport,
                                   uint8_t* response,
                                   const size_t response_capacity,
                                   size_t* response_length) {
  if (transport == NULL || transport->ops == NULL || transport->ops->recv == NULL) {
    return -1;
  }
  return transport->ops->recv(transport, response, response_capacity, response_length);
}

void mini_gnb_c_ngap_transport_close(mini_gnb_c_ngap_transport_t* transport) {
  if (transport == NULL || transport->ops == NULL || transport->ops->close == NULL) {
    return;
  }
  transport->ops->close(transport);
}
