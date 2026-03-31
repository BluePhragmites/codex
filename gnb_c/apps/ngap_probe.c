#include <arpa/inet.h>
#include <errno.h>
#include <linux/sctp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#ifndef IPPROTO_SCTP
#define IPPROTO_SCTP 132
#endif

#define MINI_GNB_C_NGAP_PPID 60u
#define MINI_GNB_C_DEFAULT_AMF_IP "192.168.1.10"
#define MINI_GNB_C_DEFAULT_AMF_PORT 38412u
#define MINI_GNB_C_DEFAULT_TIMEOUT_MS 5000u

/* Derived from frame 1 in gnb_ngap.pcap: NGSetupRequest from the reference srsRAN gNB. */
static const uint8_t k_ngsetup_request[] = {
    0x00, 0x15, 0x00, 0x33, 0x00, 0x00, 0x04, 0x00, 0x1b, 0x00, 0x08, 0x00, 0x64, 0xf0,
    0x99, 0x00, 0x00, 0x06, 0x6c, 0x00, 0x52, 0x40, 0x0a, 0x03, 0x80, 0x73, 0x72, 0x73,
    0x67, 0x6e, 0x62, 0x30, 0x31, 0x00, 0x66, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x64, 0xf0, 0x99, 0x00, 0x00, 0x00, 0x08, 0x00, 0x15, 0x40, 0x01, 0x60,
};

static void mini_gnb_c_print_usage(const char* program_name) {
  fprintf(stderr,
          "usage: %s [amf_ip] [amf_port] [timeout_ms]\n"
          "defaults:\n"
          "  amf_ip=%s\n"
          "  amf_port=%u\n"
          "  timeout_ms=%u\n",
          program_name,
          MINI_GNB_C_DEFAULT_AMF_IP,
          MINI_GNB_C_DEFAULT_AMF_PORT,
          MINI_GNB_C_DEFAULT_TIMEOUT_MS);
}

static int mini_gnb_c_parse_u32(const char* text, uint32_t* value) {
  char* end = NULL;
  unsigned long parsed = 0;

  if (text == NULL || value == NULL || text[0] == '\0') {
    return -1;
  }

  errno = 0;
  parsed = strtoul(text, &end, 10);
  if (errno != 0 || end == text || *end != '\0' || parsed > 0xffffffffUL) {
    return -1;
  }

  *value = (uint32_t)parsed;
  return 0;
}

static int mini_gnb_c_set_timeouts(int socket_fd, uint32_t timeout_ms) {
  struct timeval timeout;

  timeout.tv_sec = (time_t)(timeout_ms / 1000u);
  timeout.tv_usec = (suseconds_t)((timeout_ms % 1000u) * 1000u);

  if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
    perror("setsockopt(SO_RCVTIMEO)");
    return -1;
  }

  if (setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0) {
    perror("setsockopt(SO_SNDTIMEO)");
    return -1;
  }

  return 0;
}

static int mini_gnb_c_set_ngap_ppid(int socket_fd) {
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

static int mini_gnb_c_connect_with_timeout(int socket_fd,
                                           const struct sockaddr* address,
                                           socklen_t address_length,
                                           uint32_t timeout_ms) {
  int connect_result = 0;
  struct pollfd poll_fd;
  int poll_result = 0;
  int socket_error = 0;
  socklen_t socket_error_length = (socklen_t)sizeof(socket_error);
  uint32_t waited_ms = 0;

  connect_result = connect(socket_fd, address, address_length);
  if (connect_result == 0) {
    return 0;
  }
  if (errno != EINPROGRESS && errno != EALREADY && errno != EINTR) {
    perror("connect");
    return -1;
  }

  memset(&poll_fd, 0, sizeof(poll_fd));
  poll_fd.fd = socket_fd;
  poll_fd.events = POLLOUT;

  while (waited_ms < timeout_ms) {
    uint32_t step_ms = timeout_ms - waited_ms;
    if (step_ms > 250u) {
      step_ms = 250u;
    }

    poll_result = poll(&poll_fd, 1, (int)step_ms);
    if (poll_result < 0) {
      if (errno == EINTR) {
        continue;
      }
      perror("poll");
      return -1;
    }
    waited_ms += step_ms;
    if (poll_result == 0) {
      continue;
    }

    socket_error = 0;
    socket_error_length = (socklen_t)sizeof(socket_error);
    if (getsockopt(socket_fd,
                   SOL_SOCKET,
                   SO_ERROR,
                   &socket_error,
                   &socket_error_length) != 0) {
      perror("getsockopt(SO_ERROR)");
      return -1;
    }
    if (socket_error == 0 || socket_error == EISCONN) {
      return 0;
    }
    if (socket_error == EINPROGRESS || socket_error == EALREADY) {
      continue;
    }

    errno = socket_error;
    perror("connect");
    return -1;
  }

  fprintf(stderr, "connect timed out after %u ms\n", timeout_ms);
  return -1;
}

static void mini_gnb_c_print_hex(const uint8_t* bytes, size_t length) {
  size_t index = 0;

  for (index = 0; index < length; ++index) {
    printf("%02x", bytes[index]);
    if (index + 1u < length) {
      putchar(' ');
    }
  }
  putchar('\n');
}

static int mini_gnb_c_is_ngsetup_response(const uint8_t* bytes, size_t length) {
  if (bytes == NULL || length < 4u) {
    return 0;
  }

  return bytes[0] == 0x20u && bytes[1] == 0x15u;
}

static int mini_gnb_c_probe_socket_type(const char* socket_name,
                                        int socket_type,
                                        const char* amf_ip,
                                        uint32_t amf_port,
                                        uint32_t timeout_ms) {
  int socket_fd = -1;
  struct sockaddr_in amf_addr;
  uint8_t response[4096];
  ssize_t bytes_sent = 0;
  ssize_t bytes_received = 0;
  int send_flags = 0;

  memset(&amf_addr, 0, sizeof(amf_addr));
  amf_addr.sin_family = AF_INET;
  amf_addr.sin_port = htons((uint16_t)amf_port);
  if (inet_pton(AF_INET, amf_ip, &amf_addr.sin_addr) != 1) {
    fprintf(stderr, "invalid amf_ip: %s\n", amf_ip);
    return 1;
  }

  socket_fd = socket(AF_INET, socket_type, IPPROTO_SCTP);
  if (socket_fd < 0) {
    perror("socket(SCTP)");
    return 1;
  }

  if (mini_gnb_c_set_timeouts(socket_fd, timeout_ms) != 0) {
    close(socket_fd);
    return 1;
  }

  if (mini_gnb_c_set_ngap_ppid(socket_fd) != 0) {
    close(socket_fd);
    return 1;
  }

  printf("Connecting to AMF %s:%u via SCTP %s...\n", amf_ip, amf_port, socket_name);
  if (mini_gnb_c_connect_with_timeout(socket_fd,
                                      (const struct sockaddr*)&amf_addr,
                                      sizeof(amf_addr),
                                      timeout_ms) != 0) {
    close(socket_fd);
    return 1;
  }

#ifdef MSG_NOSIGNAL
  send_flags = MSG_NOSIGNAL;
#endif

  printf("Sending NGSetupRequest (%zu bytes) over %s...\n", sizeof(k_ngsetup_request), socket_name);
  bytes_sent = send(socket_fd, k_ngsetup_request, sizeof(k_ngsetup_request), send_flags);
  if (bytes_sent < 0) {
    perror("send");
    close(socket_fd);
    return 1;
  }
  if ((size_t)bytes_sent != sizeof(k_ngsetup_request)) {
    fprintf(stderr, "short send: expected %zu, got %zd\n", sizeof(k_ngsetup_request), bytes_sent);
    close(socket_fd);
    return 1;
  }

  bytes_received = recv(socket_fd, response, sizeof(response), 0);
  if (bytes_received < 0) {
    perror("recv");
    close(socket_fd);
    return 1;
  }

  printf("Received %zd bytes from AMF.\n", bytes_received);
  printf("Response hex:\n");
  mini_gnb_c_print_hex(response, (size_t)bytes_received);

  if (!mini_gnb_c_is_ngsetup_response(response, (size_t)bytes_received)) {
    fprintf(stderr, "response is not recognized as NGSetupResponse\n");
    close(socket_fd);
    return 2;
  }

  printf("NGSetupResponse detected.\n");
  close(socket_fd);
  return 0;
}

int main(int argc, char** argv) {
  const char* amf_ip = MINI_GNB_C_DEFAULT_AMF_IP;
  uint32_t amf_port = MINI_GNB_C_DEFAULT_AMF_PORT;
  uint32_t timeout_ms = MINI_GNB_C_DEFAULT_TIMEOUT_MS;

  signal(SIGPIPE, SIG_IGN);

  if (argc > 4) {
    mini_gnb_c_print_usage(argv[0]);
    return 1;
  }

  if (argc >= 2 && argv[1] != NULL && argv[1][0] != '\0') {
    amf_ip = argv[1];
  }
  if (argc >= 3 && mini_gnb_c_parse_u32(argv[2], &amf_port) != 0) {
    fprintf(stderr, "invalid amf_port: %s\n", argv[2]);
    return 1;
  }
  if (argc >= 4 && mini_gnb_c_parse_u32(argv[3], &timeout_ms) != 0) {
    fprintf(stderr, "invalid timeout_ms: %s\n", argv[3]);
    return 1;
  }

  if (mini_gnb_c_probe_socket_type("SOCK_STREAM", SOCK_STREAM, amf_ip, amf_port, timeout_ms) == 0) {
    return 0;
  }

  fprintf(stderr, "SOCK_STREAM probe failed, trying SOCK_SEQPACKET for diagnostics...\n");
  return mini_gnb_c_probe_socket_type("SOCK_SEQPACKET", SOCK_SEQPACKET, amf_ip, amf_port, timeout_ms);
}
