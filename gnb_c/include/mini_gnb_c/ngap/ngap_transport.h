#ifndef MINI_GNB_C_NGAP_NGAP_TRANSPORT_H
#define MINI_GNB_C_NGAP_NGAP_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

typedef struct mini_gnb_c_ngap_transport mini_gnb_c_ngap_transport_t;

typedef struct {
  int (*connect)(mini_gnb_c_ngap_transport_t* transport,
                 const char* amf_ip,
                 uint32_t amf_port,
                 uint32_t timeout_ms);
  int (*send)(mini_gnb_c_ngap_transport_t* transport, const uint8_t* bytes, size_t length);
  int (*recv)(mini_gnb_c_ngap_transport_t* transport,
              uint8_t* response,
              size_t response_capacity,
              size_t* response_length);
  void (*close)(mini_gnb_c_ngap_transport_t* transport);
} mini_gnb_c_ngap_transport_ops_t;

struct mini_gnb_c_ngap_transport {
  int socket_fd;
  const mini_gnb_c_ngap_transport_ops_t* ops;
  void* user_data;
};

void mini_gnb_c_ngap_transport_init(mini_gnb_c_ngap_transport_t* transport);
void mini_gnb_c_ngap_transport_set_ops(mini_gnb_c_ngap_transport_t* transport,
                                       const mini_gnb_c_ngap_transport_ops_t* ops,
                                       void* user_data);
int mini_gnb_c_ngap_transport_connect(mini_gnb_c_ngap_transport_t* transport,
                                      const char* amf_ip,
                                      uint32_t amf_port,
                                      uint32_t timeout_ms);
int mini_gnb_c_ngap_transport_send(mini_gnb_c_ngap_transport_t* transport,
                                   const uint8_t* bytes,
                                   size_t length);
int mini_gnb_c_ngap_transport_recv(mini_gnb_c_ngap_transport_t* transport,
                                   uint8_t* response,
                                   size_t response_capacity,
                                   size_t* response_length);
void mini_gnb_c_ngap_transport_close(mini_gnb_c_ngap_transport_t* transport);

#endif
