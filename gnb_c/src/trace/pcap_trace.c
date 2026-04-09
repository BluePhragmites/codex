#include "mini_gnb_c/trace/pcap_trace.h"

#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

static uint16_t mini_gnb_c_trace_checksum16(const uint8_t* data, size_t length) {
  uint32_t sum = 0u;
  size_t index = 0u;

  if (data == NULL) {
    return 0u;
  }

  while (index + 1u < length) {
    sum += ((uint32_t)data[index] << 8u) | (uint32_t)data[index + 1u];
    index += 2u;
  }
  if (index < length) {
    sum += ((uint32_t)data[index] << 8u);
  }

  while ((sum >> 16u) != 0u) {
    sum = (sum & 0xffffu) + (sum >> 16u);
  }

  return (uint16_t)(~sum & 0xffffu);
}

static int mini_gnb_c_trace_ensure_parent_dir(const char* path) {
  char temp[MINI_GNB_C_MAX_PATH];
  size_t index = 0u;

  if (path == NULL) {
    return -1;
  }

  (void)snprintf(temp, sizeof(temp), "%s", path);
  for (index = 1u; temp[index] != '\0'; ++index) {
    if (temp[index] == '/' || temp[index] == '\\') {
      const char saved = temp[index];
      temp[index] = '\0';
      if (strlen(temp) > 0u && mkdir(temp, 0777) != 0 && errno != EEXIST) {
        temp[index] = saved;
        return -1;
      }
      temp[index] = saved;
    }
  }
  return 0;
}

static int mini_gnb_c_trace_build_udp_ipv4_packet(const char* src_ip,
                                                  uint16_t src_port,
                                                  const char* dst_ip,
                                                  uint16_t dst_port,
                                                  const uint8_t* udp_payload,
                                                  size_t udp_payload_length,
                                                  uint8_t* packet,
                                                  size_t packet_capacity,
                                                  size_t* packet_length) {
  uint32_t src_addr = htonl(INADDR_ANY);
  uint32_t dst_addr = htonl(INADDR_ANY);
  uint16_t total_length = 0u;
  uint16_t udp_length = 0u;

  if (src_ip == NULL || dst_ip == NULL || udp_payload == NULL || packet == NULL || packet_length == NULL) {
    return -1;
  }
  if (inet_pton(AF_INET, src_ip, &src_addr) != 1 || inet_pton(AF_INET, dst_ip, &dst_addr) != 1) {
    return -1;
  }

  total_length = (uint16_t)(20u + 8u + udp_payload_length);
  udp_length = (uint16_t)(8u + udp_payload_length);
  if ((size_t)total_length > packet_capacity) {
    return -1;
  }

  memset(packet, 0, total_length);
  packet[0] = 0x45u;
  packet[2] = (uint8_t)(total_length >> 8u);
  packet[3] = (uint8_t)(total_length & 0xffu);
  packet[4] = 0x43u;
  packet[5] = 0x21u;
  packet[8] = 64u;
  packet[9] = 17u;
  memcpy(packet + 12u, &src_addr, 4u);
  memcpy(packet + 16u, &dst_addr, 4u);
  {
    const uint16_t ip_checksum = mini_gnb_c_trace_checksum16(packet, 20u);
    packet[10] = (uint8_t)(ip_checksum >> 8u);
    packet[11] = (uint8_t)(ip_checksum & 0xffu);
  }

  packet[20] = (uint8_t)(src_port >> 8u);
  packet[21] = (uint8_t)(src_port & 0xffu);
  packet[22] = (uint8_t)(dst_port >> 8u);
  packet[23] = (uint8_t)(dst_port & 0xffu);
  packet[24] = (uint8_t)(udp_length >> 8u);
  packet[25] = (uint8_t)(udp_length & 0xffu);
  memcpy(packet + 28u, udp_payload, udp_payload_length);
  *packet_length = total_length;
  return 0;
}

void mini_gnb_c_pcap_writer_init(mini_gnb_c_pcap_writer_t* writer) {
  if (writer == NULL) {
    return;
  }

  memset(writer, 0, sizeof(*writer));
}

int mini_gnb_c_pcap_writer_open(mini_gnb_c_pcap_writer_t* writer, const char* path, uint32_t linktype) {
  uint8_t global_header[24];

  if (writer == NULL || path == NULL || path[0] == '\0') {
    return -1;
  }
  if (mini_gnb_c_trace_ensure_parent_dir(path) != 0) {
    return -1;
  }

  mini_gnb_c_pcap_writer_close(writer);
  writer->file = fopen(path, "wb");
  if (writer->file == NULL) {
    return -1;
  }
  writer->linktype = linktype;
  (void)snprintf(writer->path, sizeof(writer->path), "%s", path);

  memset(global_header, 0, sizeof(global_header));
  global_header[0] = 0xd4u;
  global_header[1] = 0xc3u;
  global_header[2] = 0xb2u;
  global_header[3] = 0xa1u;
  global_header[4] = 0x02u;
  global_header[6] = 0x04u;
  global_header[16] = 0xffu;
  global_header[17] = 0xffu;
  global_header[20] = (uint8_t)(linktype & 0xffu);
  global_header[21] = (uint8_t)((linktype >> 8u) & 0xffu);

  if (fwrite(global_header, 1u, sizeof(global_header), writer->file) != sizeof(global_header)) {
    mini_gnb_c_pcap_writer_close(writer);
    return -1;
  }

  return 0;
}

void mini_gnb_c_pcap_writer_close(mini_gnb_c_pcap_writer_t* writer) {
  if (writer == NULL) {
    return;
  }
  if (writer->file != NULL) {
    fclose(writer->file);
  }
  writer->file = NULL;
  writer->linktype = 0u;
}

bool mini_gnb_c_pcap_writer_is_open(const mini_gnb_c_pcap_writer_t* writer) {
  return writer != NULL && writer->file != NULL;
}

const char* mini_gnb_c_pcap_writer_path(const mini_gnb_c_pcap_writer_t* writer) {
  if (writer == NULL || writer->path[0] == '\0') {
    return "";
  }
  return writer->path;
}

int mini_gnb_c_pcap_writer_write(mini_gnb_c_pcap_writer_t* writer, const uint8_t* bytes, size_t length) {
  struct timeval now;
  uint8_t record_header[16];
  uint32_t ts_sec = 0u;
  uint32_t ts_usec = 0u;
  uint32_t captured_length = 0u;

  if (!mini_gnb_c_pcap_writer_is_open(writer) || bytes == NULL || length == 0u || length > 0xffffffffu) {
    return -1;
  }
  if (gettimeofday(&now, NULL) != 0) {
    return -1;
  }

  ts_sec = (uint32_t)now.tv_sec;
  ts_usec = (uint32_t)now.tv_usec;
  captured_length = (uint32_t)length;
  memset(record_header, 0, sizeof(record_header));
  memcpy(record_header + 0u, &ts_sec, sizeof(ts_sec));
  memcpy(record_header + 4u, &ts_usec, sizeof(ts_usec));
  memcpy(record_header + 8u, &captured_length, sizeof(captured_length));
  memcpy(record_header + 12u, &captured_length, sizeof(captured_length));

  if (fwrite(record_header, 1u, sizeof(record_header), writer->file) != sizeof(record_header) ||
      fwrite(bytes, 1u, length, writer->file) != length) {
    return -1;
  }
  fflush(writer->file);
  return 0;
}

int mini_gnb_c_pcap_writer_write_udp_ipv4(mini_gnb_c_pcap_writer_t* writer,
                                          const char* src_ip,
                                          uint16_t src_port,
                                          const char* dst_ip,
                                          uint16_t dst_port,
                                          const uint8_t* udp_payload,
                                          size_t udp_payload_length) {
  uint8_t packet[MINI_GNB_C_TRACE_MAX_PACKET];
  size_t packet_length = 0u;

  if (!mini_gnb_c_pcap_writer_is_open(writer)) {
    return -1;
  }
  if (mini_gnb_c_trace_build_udp_ipv4_packet(src_ip,
                                             src_port,
                                             dst_ip,
                                             dst_port,
                                             udp_payload,
                                             udp_payload_length,
                                             packet,
                                             sizeof(packet),
                                             &packet_length) != 0) {
    return -1;
  }
  return mini_gnb_c_pcap_writer_write(writer, packet, packet_length);
}
