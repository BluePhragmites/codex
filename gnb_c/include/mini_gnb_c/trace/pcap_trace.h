#ifndef MINI_GNB_C_TRACE_PCAP_TRACE_H
#define MINI_GNB_C_TRACE_PCAP_TRACE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "mini_gnb_c/common/types.h"

#define MINI_GNB_C_PCAP_LINKTYPE_RAW 101u
#define MINI_GNB_C_PCAP_LINKTYPE_USER5 152u
#define MINI_GNB_C_TRACE_MAX_PACKET 4096u

typedef struct {
  FILE* file;
  uint32_t linktype;
  char path[MINI_GNB_C_MAX_PATH];
} mini_gnb_c_pcap_writer_t;

void mini_gnb_c_pcap_writer_init(mini_gnb_c_pcap_writer_t* writer);
int mini_gnb_c_pcap_writer_open(mini_gnb_c_pcap_writer_t* writer, const char* path, uint32_t linktype);
void mini_gnb_c_pcap_writer_close(mini_gnb_c_pcap_writer_t* writer);
bool mini_gnb_c_pcap_writer_is_open(const mini_gnb_c_pcap_writer_t* writer);
const char* mini_gnb_c_pcap_writer_path(const mini_gnb_c_pcap_writer_t* writer);
int mini_gnb_c_pcap_writer_write(mini_gnb_c_pcap_writer_t* writer, const uint8_t* bytes, size_t length);
int mini_gnb_c_pcap_writer_write_udp_ipv4(mini_gnb_c_pcap_writer_t* writer,
                                          const char* src_ip,
                                          uint16_t src_port,
                                          const char* dst_ip,
                                          uint16_t dst_port,
                                          const uint8_t* udp_payload,
                                          size_t udp_payload_length);

#endif
