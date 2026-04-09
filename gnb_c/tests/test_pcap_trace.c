#include <string.h>

#include "test_helpers.h"

#include "mini_gnb_c/common/json_utils.h"
#include "mini_gnb_c/trace/pcap_trace.h"

void test_pcap_trace_writes_payload_and_udp_ipv4(void) {
  static const uint8_t k_payload[] = {0x00u, 0x15u, 0x00u, 0x33u};
  static const uint8_t k_gtpu_payload[] = {0x34u, 0xffu, 0x00u, 0x08u, 0x11u, 0x22u, 0x33u, 0x44u};
  char output_dir[MINI_GNB_C_MAX_PATH];
  char ngap_path[MINI_GNB_C_MAX_PATH];
  char gtpu_path[MINI_GNB_C_MAX_PATH];
  mini_gnb_c_pcap_writer_t writer;

  mini_gnb_c_make_output_dir("test_pcap_trace", output_dir, sizeof(output_dir));
  mini_gnb_c_require(mini_gnb_c_join_path(output_dir, "ngap_trace.pcap", ngap_path, sizeof(ngap_path)) == 0,
                     "expected NGAP pcap path");
  mini_gnb_c_require(mini_gnb_c_join_path(output_dir, "gtpu_trace.pcap", gtpu_path, sizeof(gtpu_path)) == 0,
                     "expected GTP-U pcap path");

  mini_gnb_c_pcap_writer_init(&writer);
  mini_gnb_c_require(mini_gnb_c_pcap_writer_open(&writer, ngap_path, MINI_GNB_C_PCAP_LINKTYPE_USER5) == 0,
                     "expected NGAP pcap open");
  mini_gnb_c_require(mini_gnb_c_pcap_writer_write(&writer, k_payload, sizeof(k_payload)) == 0,
                     "expected NGAP pcap packet write");
  mini_gnb_c_pcap_writer_close(&writer);
  mini_gnb_c_require(mini_gnb_c_path_exists(ngap_path), "expected NGAP pcap file");
  mini_gnb_c_require(mini_gnb_c_file_size(ngap_path) == 24u + 16u + sizeof(k_payload),
                     "expected one USER5 payload record");

  mini_gnb_c_pcap_writer_init(&writer);
  mini_gnb_c_require(mini_gnb_c_pcap_writer_open(&writer, gtpu_path, MINI_GNB_C_PCAP_LINKTYPE_RAW) == 0,
                     "expected GTP-U pcap open");
  mini_gnb_c_require(mini_gnb_c_pcap_writer_write_udp_ipv4(&writer,
                                                           "127.0.0.1",
                                                           2152u,
                                                           "127.0.0.7",
                                                           2152u,
                                                           k_gtpu_payload,
                                                           sizeof(k_gtpu_payload)) == 0,
                     "expected GTP-U UDP/IPv4 packet write");
  mini_gnb_c_pcap_writer_close(&writer);
  mini_gnb_c_require(mini_gnb_c_path_exists(gtpu_path), "expected GTP-U pcap file");
  mini_gnb_c_require(mini_gnb_c_file_size(gtpu_path) == 24u + 16u + 20u + 8u + sizeof(k_gtpu_payload),
                     "expected one RAW IPv4/UDP packet record");
}
