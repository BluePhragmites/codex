#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <linux/sctp.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "mini_gnb_c/core/core_session.h"
#include "mini_gnb_c/n3/gtpu_tunnel.h"

#ifndef IPPROTO_SCTP
#define IPPROTO_SCTP 132
#endif

#ifndef MINI_GNB_C_SOURCE_DIR
#define MINI_GNB_C_SOURCE_DIR "."
#endif

#define MINI_GNB_C_NGAP_PPID 60u
#define MINI_GNB_C_DEFAULT_AMF_IP "192.168.1.10"
#define MINI_GNB_C_DEFAULT_AMF_PORT 38412u
#define MINI_GNB_C_DEFAULT_UPF_IP "127.0.0.7"
#define MINI_GNB_C_DEFAULT_UPF_PORT 2152u
#define MINI_GNB_C_DEFAULT_GPDU_DST_IP "10.45.0.1"
#define MINI_GNB_C_DEFAULT_NGAP_TRACE_PCAP MINI_GNB_C_SOURCE_DIR "/out/ngap_probe_ngap_runtime.pcap"
#define MINI_GNB_C_DEFAULT_GTPU_TRACE_PCAP MINI_GNB_C_SOURCE_DIR "/out/ngap_probe_gtpu_runtime.pcap"
#define MINI_GNB_C_DEFAULT_TIMEOUT_MS 5000u
#define MINI_GNB_C_DEFAULT_REPLAY_PCAP MINI_GNB_C_SOURCE_DIR "/gnb_ngap.pcap"
#define MINI_GNB_C_MAX_PCAP_FRAMES 64u
#define MINI_GNB_C_DEFAULT_IMSI "460991234567898"
#define MINI_GNB_C_DEFAULT_KEY "11111111 11111111 11111111 11111111"
#define MINI_GNB_C_DEFAULT_OPC "11111111 11111111 11111111 11111111"
#define MINI_GNB_C_DEFAULT_AUTH_AMF "8000"
#define MINI_GNB_C_DEFAULT_MCC "460"
#define MINI_GNB_C_DEFAULT_MNC "99"
#define MINI_GNB_C_NAS_MAX_LEN 512u
#define MINI_GNB_C_KAUSF_LEN 32u
#define MINI_GNB_C_KSEAF_LEN 32u
#define MINI_GNB_C_KAMF_LEN 32u
#define MINI_GNB_C_KNAS_LEN 16u
#define MINI_GNB_C_NAS_BEARER_3GPP 1u
#define MINI_GNB_C_NAS_DIRECTION_UPLINK 0u
#define MINI_GNB_C_KDF_NAS_ENC_ALG 0x01u
#define MINI_GNB_C_KDF_NAS_INT_ALG 0x02u
#define MINI_GNB_C_NAS_ALG_NEA0 0u
#define MINI_GNB_C_NAS_ALG_NIA2 2u
#define MINI_GNB_C_PCAP_LINKTYPE_RAW 101u
#define MINI_GNB_C_PCAP_LINKTYPE_USER5 152u

int milenage_f2345(const uint8_t *opc, const uint8_t *k,
                   const uint8_t *_rand, uint8_t *res, uint8_t *ck, uint8_t *ik,
                   uint8_t *ak, uint8_t *akstar);
void ogs_kdf_kausf(uint8_t *ck, uint8_t *ik,
                   char *serving_network_name, uint8_t *autn,
                   uint8_t *kausf);
void ogs_kdf_kseaf(char *serving_network_name, const uint8_t *kausf, uint8_t *kseaf);
void ogs_kdf_kamf(const char *supi, const uint8_t *abba, uint8_t abba_len,
                  const uint8_t *kseaf, uint8_t *kamf);
void ogs_kdf_nas_5gs(uint8_t algorithm_type_distinguishers,
                     uint8_t algorithm_identity, const uint8_t *kamf, uint8_t *knas);
void ogs_kdf_xres_star(uint8_t *ck, uint8_t *ik,
                       char *serving_network_name, uint8_t *rand,
                       uint8_t *xres, size_t xres_len,
                       uint8_t *xres_star);
int ogs_aes_cmac_calculate(uint8_t *cmac, const uint8_t *key,
                           const uint8_t *msg, const uint32_t len);

/* Derived from frame 1 in gnb_ngap.pcap: NGSetupRequest from the reference srsRAN gNB. */
static const uint8_t k_ngsetup_request[] = {
    0x00, 0x15, 0x00, 0x33, 0x00, 0x00, 0x04, 0x00, 0x1b, 0x00, 0x08, 0x00, 0x64, 0xf0,
    0x99, 0x00, 0x00, 0x06, 0x6c, 0x00, 0x52, 0x40, 0x0a, 0x03, 0x80, 0x73, 0x72, 0x73,
    0x67, 0x6e, 0x62, 0x30, 0x31, 0x00, 0x66, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x64, 0xf0, 0x99, 0x00, 0x00, 0x00, 0x08, 0x00, 0x15, 0x40, 0x01, 0x60,
};

static const uint8_t k_ran_ue_ngap_id[] = {0x00u, 0x00u};
static const uint8_t k_user_location_information_nr[] = {
    0x40, 0x64, 0xf0, 0x99, 0x00, 0x06, 0x6c, 0x00, 0x00, 0x64, 0xf0, 0x99, 0x00, 0x00, 0x01,
};
static const uint8_t k_rrc_establishment_cause_mo_signalling[] = {0x18u};
static const uint8_t k_ue_context_request_requested[] = {0x00u};
static const uint8_t k_initial_registration_request_nas[] = {
    0x7e, 0x01, 0xda, 0xb8, 0x93, 0xa0, 0x04, 0x7e, 0x00, 0x41, 0x29, 0x00, 0x0b, 0xf2,
    0x64, 0xf0, 0x99, 0x02, 0x00, 0x40, 0xc0, 0x00, 0x06, 0x01, 0x2e, 0x02, 0xf0, 0xf0,
    0x71, 0x00, 0x2e, 0x7e, 0x00, 0x41, 0x29, 0x00, 0x0b, 0xf2, 0x64, 0xf0, 0x99, 0x02,
    0x00, 0x40, 0xc0, 0x00, 0x06, 0x01, 0x10, 0x01, 0x07, 0x2e, 0x02, 0xf0, 0xf0, 0x52,
    0x64, 0xf0, 0x99, 0x00, 0x00, 0x01, 0x17, 0x07, 0xf0, 0xf0, 0xc0, 0xc0, 0x1d, 0x80,
    0x30, 0x18, 0x01, 0x00, 0x53, 0x01, 0x01,
};
static const uint8_t k_identity_response_nas[] = {
    0x7e, 0x01, 0xae, 0x00, 0xf9, 0x48, 0x05, 0x7e, 0x00, 0x5c, 0x00, 0x0d, 0x01, 0x64,
    0xf0, 0x99, 0xf0, 0xff, 0x00, 0x00, 0x21, 0x43, 0x65, 0x87, 0x89,
};
static const uint8_t k_security_mode_complete_nas_template[] = {
    0x7e, 0x04, 0x1f, 0x7c, 0xe9, 0x75, 0x00, 0x7e, 0x00, 0x5e, 0x77, 0x00, 0x09, 0x85,
    0x26, 0x61, 0x09, 0x56, 0x16, 0x39, 0x78, 0xf8, 0x71, 0x00, 0x2e, 0x7e, 0x00, 0x41,
    0x39, 0x00, 0x0b, 0xf2, 0x64, 0xf0, 0x99, 0x02, 0x00, 0x40, 0xc0, 0x00, 0x06, 0x01,
    0x10, 0x01, 0x07, 0x2e, 0x02, 0xf0, 0xf0, 0x52, 0x64, 0xf0, 0x99, 0x00, 0x00, 0x01,
    0x17, 0x07, 0xf0, 0xf0, 0xc0, 0xc0, 0x1d, 0x80, 0x30, 0x18, 0x01, 0x00, 0x53, 0x01,
    0x01,
};
static const uint8_t k_registration_complete_nas_template[] = {
    0x7e, 0x02, 0x0b, 0x94, 0x35, 0xf4, 0x01, 0x7e, 0x00, 0x43,
};
static const uint8_t k_pdu_session_establishment_request_nas_template[] = {
    0x7e, 0x02, 0x7e, 0xa9, 0x5c, 0x08, 0x02, 0x7e, 0x00, 0x67, 0x01, 0x00, 0x33, 0x2e,
    0x01, 0x01, 0xc1, 0xff, 0xff, 0x91, 0x28, 0x01, 0x00, 0x55, 0x10, 0x00, 0x7b, 0x00,
    0x23, 0x80, 0x80, 0x21, 0x10, 0x01, 0x01, 0x00, 0x10, 0x81, 0x06, 0x00, 0x00, 0x00,
    0x00, 0x83, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x0d, 0x00, 0x00,
    0x05, 0x00, 0x00, 0x11, 0x00, 0x00, 0x10, 0x00, 0x12, 0x01, 0x81, 0x25, 0x08, 0x07,
    0x64, 0x65, 0x66, 0x61, 0x75, 0x6c, 0x74,
};
static const uint8_t k_pdu_session_resource_setup_response_list[] = {
    0x00, 0x00, 0x01, 0x0d, 0x00, 0x03, 0xe0, 0x7f, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x01,
};

typedef enum {
  MINI_GNB_C_MODE_SETUP = 0,
  MINI_GNB_C_MODE_REPLAY = 1,
} mini_gnb_c_mode_t;

typedef struct {
  uint8_t* data;
  size_t length;
} mini_gnb_c_pcap_frame_t;

typedef struct {
  uint32_t frame_number;
  const char* label;
  uint8_t expected_pdu_type;
  uint8_t expected_proc_code;
  int wait_for_response;
  uint32_t post_send_delay_ms;
} mini_gnb_c_replay_step_t;

typedef struct {
  mini_gnb_c_mode_t mode;
  const char* amf_ip;
  uint32_t amf_port;
  uint32_t timeout_ms;
  const char* replay_pcap_path;
  const char* imsi;
  const char* key_hex;
  const char* opc_hex;
  const char* auth_amf_hex;
  const char* mcc;
  const char* mnc;
  const char* upf_ip;
  uint32_t upf_port;
  const char* gpdu_dst_ip;
  const char* ngap_trace_pcap_path;
  const char* gtpu_trace_pcap_path;
} mini_gnb_c_probe_options_t;

typedef struct {
  uint8_t rand[16];
  uint8_t autn[16];
  uint8_t ck[16];
  uint8_t ik[16];
  uint8_t ak[6];
  uint8_t sqn_xor_ak[6];
  uint8_t sqn[6];
  uint8_t abba[32];
  size_t abba_len;
  uint8_t res[16];
  size_t res_len;
  uint8_t res_star[16];
  size_t res_star_len;
  uint8_t kausf[MINI_GNB_C_KAUSF_LEN];
  uint8_t kseaf[MINI_GNB_C_KSEAF_LEN];
  uint8_t kamf[MINI_GNB_C_KAMF_LEN];
  uint8_t knas_enc[MINI_GNB_C_KNAS_LEN];
  uint8_t knas_int[MINI_GNB_C_KNAS_LEN];
  uint8_t ciphering_algorithm;
  uint8_t integrity_algorithm;
} mini_gnb_c_aka_context_t;

typedef struct {
  const uint8_t* bytes;
  size_t length;
} mini_gnb_c_octets_t;

typedef struct {
  uint16_t id;
  uint8_t criticality;
  mini_gnb_c_octets_t value;
} mini_gnb_c_ngap_ie_t;

typedef struct {
  const uint8_t* bytes;
  size_t length;
  uint32_t count;
  uint32_t index;
  size_t next_offset;
} mini_gnb_c_ie_sequence_t;

typedef struct {
  FILE* file;
  uint32_t linktype;
} mini_gnb_c_pcap_writer_t;

typedef struct {
  uint16_t id;
  uint8_t criticality;
  const uint8_t* value;
  size_t value_length;
} mini_gnb_c_build_ie_t;

static int mini_gnb_c_set_timeouts(int socket_fd, uint32_t timeout_ms);
static mini_gnb_c_pcap_writer_t g_mini_gnb_c_ngap_trace_writer;
static mini_gnb_c_pcap_writer_t g_mini_gnb_c_gtpu_trace_writer;

static const mini_gnb_c_replay_step_t k_replay_steps[] = {
    {1u, "NGSetupRequest", 0x20u, 0x15u, 1, 0u},
    {3u, "InitialUEMessage", 0x00u, 0x04u, 1, 0u},
    {5u, "UplinkNASTransport / IdentityResponse", 0x00u, 0x04u, 1, 0u},
    {7u, "UplinkNASTransport / AuthenticationResponse", 0x00u, 0x04u, 1, 0u},
    {9u, "UplinkNASTransport / SecurityModeComplete", 0x00u, 0x0eu, 1, 0u},
    {11u, "InitialContextSetupResponse", 0x00u, 0x00u, 0, 20u},
    {12u, "UplinkNASTransport / RegistrationComplete", 0x00u, 0x04u, 1, 0u},
    {14u, "UplinkNASTransport / PDUSessionEstablishmentRequest", 0x00u, 0x1du, 1, 0u},
    {16u, "PDUSessionResourceSetupResponse", 0x00u, 0x00u, 0, 0u},
};

static void mini_gnb_c_print_usage(const char* program_name) {
  fprintf(stderr,
          "usage: %s [--replay] [--pcap path] [--upf-ip ip] [--upf-port port] "
          "[--gpdu-dst-ip ip] [--ngap-trace-pcap path] [--gtpu-trace-pcap path] "
          "[amf_ip] [amf_port] [timeout_ms]\n"
          "defaults:\n"
          "  amf_ip=%s\n"
          "  amf_port=%u\n"
          "  timeout_ms=%u\n"
          "  replay_pcap=%s\n"
          "  imsi=%s\n"
          "  key=%s\n"
          "  opc=%s\n"
          "  auth_amf=%s\n"
          "  mcc=%s\n"
          "  mnc=%s\n"
          "  upf_ip=%s\n"
          "  upf_port=%u\n"
          "  gpdu_dst_ip=%s\n"
          "  ngap_trace_pcap=%s\n"
          "  gtpu_trace_pcap=%s\n"
          "\n"
          "modes:\n"
          "  default : send one NGSetupRequest and wait for NGSetupResponse\n"
          "  --replay: drive the same N2 attach/session flow as the reference gnb_ngap.pcap,\n"
          "            but build the gNB-originated uplink NGAP messages locally at runtime\n",
          program_name,
          MINI_GNB_C_DEFAULT_AMF_IP,
          MINI_GNB_C_DEFAULT_AMF_PORT,
          MINI_GNB_C_DEFAULT_TIMEOUT_MS,
          MINI_GNB_C_DEFAULT_REPLAY_PCAP,
          MINI_GNB_C_DEFAULT_IMSI,
          MINI_GNB_C_DEFAULT_KEY,
          MINI_GNB_C_DEFAULT_OPC,
          MINI_GNB_C_DEFAULT_AUTH_AMF,
          MINI_GNB_C_DEFAULT_MCC,
          MINI_GNB_C_DEFAULT_MNC,
          MINI_GNB_C_DEFAULT_UPF_IP,
          MINI_GNB_C_DEFAULT_UPF_PORT,
          MINI_GNB_C_DEFAULT_GPDU_DST_IP,
          MINI_GNB_C_DEFAULT_NGAP_TRACE_PCAP,
          MINI_GNB_C_DEFAULT_GTPU_TRACE_PCAP);
}

static const char* mini_gnb_c_pdu_type_name(uint8_t pdu_type) {
  switch (pdu_type) {
    case 0x00u:
      return "initiatingMessage";
    case 0x20u:
      return "successfulOutcome";
    case 0x40u:
      return "unsuccessfulOutcome";
    default:
      return "unknown";
  }
}

static const char* mini_gnb_c_procedure_name(uint8_t procedure_code) {
  switch (procedure_code) {
    case 0x04u:
      return "DownlinkNASTransport";
    case 0x09u:
      return "ErrorIndication";
    case 0x0eu:
      return "InitialContextSetup";
    case 0x0fu:
      return "InitialUEMessage";
    case 0x15u:
      return "NGSetup";
    case 0x1du:
      return "PDUSessionResourceSetup";
    case 0x29u:
      return "UEContextRelease";
    case 0x2eu:
      return "UplinkNASTransport";
    default:
      return "unknown";
  }
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

static uint32_t mini_gnb_c_read_u32_be(const uint8_t* bytes) {
  return ((uint32_t)bytes[0] << 24u) | ((uint32_t)bytes[1] << 16u) |
         ((uint32_t)bytes[2] << 8u) | (uint32_t)bytes[3];
}

static uint32_t mini_gnb_c_read_u32_le(const uint8_t* bytes) {
  return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8u) | ((uint32_t)bytes[2] << 16u) |
         ((uint32_t)bytes[3] << 24u);
}

static uint32_t mini_gnb_c_read_u24_be(const uint8_t* bytes) {
  return ((uint32_t)bytes[0] << 16u) | ((uint32_t)bytes[1] << 8u) | (uint32_t)bytes[2];
}

static int mini_gnb_c_decode_compact_length(const uint8_t* bytes,
                                            size_t available_length,
                                            size_t* value_out,
                                            size_t* consumed_out) {
  if (bytes == NULL || value_out == NULL || consumed_out == NULL || available_length == 0u) {
    return -1;
  }

  if (bytes[0] == 0x80u) {
    if (available_length < 2u) {
      return -1;
    }
    *value_out = (size_t)bytes[1];
    *consumed_out = 2u;
    return 0;
  }

  *value_out = (size_t)bytes[0];
  *consumed_out = 1u;
  return 0;
}

static int mini_gnb_c_encode_compact_length(uint8_t* bytes,
                                            size_t capacity,
                                            size_t value,
                                            size_t* encoded_length_out) {
  if (bytes == NULL || encoded_length_out == NULL) {
    return -1;
  }
  if (value < 128u) {
    if (capacity < 1u) {
      return -1;
    }
    bytes[0] = (uint8_t)value;
    *encoded_length_out = 1u;
    return 0;
  }
  if (value > 255u || capacity < 2u) {
    return -1;
  }

  bytes[0] = 0x80u;
  bytes[1] = (uint8_t)value;
  *encoded_length_out = 2u;
  return 0;
}

static int mini_gnb_c_build_octet_string_value(const uint8_t* octets,
                                               size_t octets_length,
                                               uint8_t* encoded_value,
                                               size_t encoded_value_capacity,
                                               size_t* encoded_value_length_out) {
  size_t length_field_size = 0u;

  if (octets == NULL || encoded_value == NULL || encoded_value_length_out == NULL) {
    return -1;
  }
  if (mini_gnb_c_encode_compact_length(encoded_value,
                                       encoded_value_capacity,
                                       octets_length,
                                       &length_field_size) != 0) {
    return -1;
  }
  if (length_field_size + octets_length > encoded_value_capacity) {
    return -1;
  }

  memcpy(encoded_value + length_field_size, octets, octets_length);
  *encoded_value_length_out = length_field_size + octets_length;
  return 0;
}

static int mini_gnb_c_build_ngap_message(uint8_t pdu_type,
                                         uint8_t procedure_code,
                                         uint8_t criticality,
                                         const mini_gnb_c_build_ie_t* ies,
                                         size_t ie_count,
                                         uint8_t* out,
                                         size_t out_capacity,
                                         size_t* out_length) {
  uint8_t payload[2048];
  size_t payload_length = 0u;
  size_t ie_index = 0u;
  size_t top_length_field_size = 0u;

  if (ies == NULL || out == NULL || out_length == NULL || ie_count > 255u) {
    return -1;
  }

  memset(payload, 0, sizeof(payload));
  payload[0] = 0x00u;
  payload[1] = 0x00u;
  payload[2] = (uint8_t)ie_count;
  payload_length = 3u;

  for (ie_index = 0u; ie_index < ie_count; ++ie_index) {
    size_t ie_length_field_size = 0u;

    if (payload_length + 5u + ies[ie_index].value_length > sizeof(payload)) {
      return -1;
    }

    payload[payload_length++] = (uint8_t)(ies[ie_index].id >> 8u);
    payload[payload_length++] = (uint8_t)(ies[ie_index].id & 0xffu);
    payload[payload_length++] = ies[ie_index].criticality;
    if (mini_gnb_c_encode_compact_length(payload + payload_length,
                                         sizeof(payload) - payload_length,
                                         ies[ie_index].value_length,
                                         &ie_length_field_size) != 0) {
      return -1;
    }
    payload_length += ie_length_field_size;
    memcpy(payload + payload_length, ies[ie_index].value, ies[ie_index].value_length);
    payload_length += ies[ie_index].value_length;
  }

  if (out_capacity < 4u) {
    return -1;
  }

  out[0] = pdu_type;
  out[1] = procedure_code;
  out[2] = criticality;
  if (mini_gnb_c_encode_compact_length(out + 3u,
                                       out_capacity - 3u,
                                       payload_length,
                                       &top_length_field_size) != 0) {
    return -1;
  }
  if (3u + top_length_field_size + payload_length > out_capacity) {
    return -1;
  }

  memcpy(out + 3u + top_length_field_size, payload, payload_length);
  *out_length = 3u + top_length_field_size + payload_length;
  return 0;
}

static int mini_gnb_c_ie_sequence_init_at(const uint8_t* bytes,
                                          size_t length,
                                          size_t count_offset,
                                          mini_gnb_c_ie_sequence_t* sequence) {
  if (bytes == NULL || sequence == NULL || count_offset + 3u > length) {
    return -1;
  }

  memset(sequence, 0, sizeof(*sequence));
  sequence->bytes = bytes;
  sequence->length = length;
  sequence->count = mini_gnb_c_read_u24_be(bytes + count_offset);
  if (sequence->count == 0u || sequence->count > 64u) {
    return -1;
  }
  sequence->next_offset = count_offset + 3u;
  return 0;
}

static int mini_gnb_c_ie_sequence_init_from_ngap_message(const uint8_t* bytes,
                                                         size_t length,
                                                         mini_gnb_c_ie_sequence_t* sequence) {
  size_t encoded_length = 0u;
  size_t length_field_bytes = 0u;
  size_t payload_offset = 0u;

  if (bytes == NULL || sequence == NULL || length < 7u) {
    return -1;
  }

  if (mini_gnb_c_decode_compact_length(bytes + 3u,
                                       length - 3u,
                                       &encoded_length,
                                       &length_field_bytes) != 0) {
    return -1;
  }

  payload_offset = 3u + length_field_bytes;
  if (payload_offset + encoded_length != length) {
    return -1;
  }

  return mini_gnb_c_ie_sequence_init_at(bytes, length, payload_offset, sequence);
}

static int mini_gnb_c_ie_sequence_next(mini_gnb_c_ie_sequence_t* sequence,
                                       mini_gnb_c_ngap_ie_t* ie_out) {
  size_t ie_length = 0u;
  size_t length_field_bytes = 0u;
  size_t value_offset = 0u;

  if (sequence == NULL || ie_out == NULL || sequence->index >= sequence->count) {
    return -1;
  }
  if (sequence->next_offset + 4u > sequence->length) {
    return -1;
  }

  ie_out->id = (uint16_t)(((uint16_t)sequence->bytes[sequence->next_offset] << 8u) |
                          (uint16_t)sequence->bytes[sequence->next_offset + 1u]);
  ie_out->criticality = sequence->bytes[sequence->next_offset + 2u];

  if (mini_gnb_c_decode_compact_length(sequence->bytes + sequence->next_offset + 3u,
                                       sequence->length - (sequence->next_offset + 3u),
                                       &ie_length,
                                       &length_field_bytes) != 0) {
    return -1;
  }

  value_offset = sequence->next_offset + 3u + length_field_bytes;
  if (value_offset + ie_length > sequence->length) {
    return -1;
  }

  ie_out->value.bytes = sequence->bytes + value_offset;
  ie_out->value.length = ie_length;
  sequence->next_offset = value_offset + ie_length;
  sequence->index += 1u;
  return 0;
}

static int mini_gnb_c_find_ie_in_sequence(const mini_gnb_c_ie_sequence_t* sequence,
                                          uint16_t ie_id,
                                          mini_gnb_c_ngap_ie_t* ie_out) {
  mini_gnb_c_ie_sequence_t cursor;
  mini_gnb_c_ngap_ie_t ie;

  if (sequence == NULL || ie_out == NULL) {
    return -1;
  }

  cursor = *sequence;
  while (mini_gnb_c_ie_sequence_next(&cursor, &ie) == 0) {
    if (ie.id == ie_id) {
      *ie_out = ie;
      return 0;
    }
  }

  return -1;
}

static int mini_gnb_c_extract_ngap_ie(const uint8_t* bytes,
                                      size_t length,
                                      uint16_t ie_id,
                                      mini_gnb_c_ngap_ie_t* ie_out) {
  mini_gnb_c_ie_sequence_t sequence;

  if (ie_out == NULL) {
    return -1;
  }
  if (mini_gnb_c_ie_sequence_init_from_ngap_message(bytes, length, &sequence) != 0) {
    return -1;
  }
  return mini_gnb_c_find_ie_in_sequence(&sequence, ie_id, ie_out);
}

static int mini_gnb_c_decode_octet_string(const mini_gnb_c_octets_t* encoded_value,
                                          mini_gnb_c_octets_t* decoded_value) {
  size_t payload_length = 0u;
  size_t length_field_bytes = 0u;

  if (encoded_value == NULL || decoded_value == NULL || encoded_value->bytes == NULL) {
    return -1;
  }

  if (mini_gnb_c_decode_compact_length(encoded_value->bytes,
                                       encoded_value->length,
                                       &payload_length,
                                       &length_field_bytes) != 0) {
    return -1;
  }
  if (length_field_bytes + payload_length != encoded_value->length) {
    return -1;
  }

  decoded_value->bytes = encoded_value->bytes + length_field_bytes;
  decoded_value->length = payload_length;
  return 0;
}

static int mini_gnb_c_find_tail_ie_sequence(const uint8_t* bytes,
                                            size_t length,
                                            const uint16_t* required_ids,
                                            size_t required_id_count,
                                            mini_gnb_c_ie_sequence_t* sequence_out) {
  size_t count_offset = 0u;
  int found = 0;

  if (bytes == NULL || sequence_out == NULL) {
    return -1;
  }

  for (count_offset = 0u; count_offset + 3u <= length; ++count_offset) {
    mini_gnb_c_ie_sequence_t sequence;
    mini_gnb_c_ie_sequence_t cursor;
    mini_gnb_c_ngap_ie_t ie;
    size_t required_hits = 0u;
    size_t id_index = 0u;

    if (mini_gnb_c_ie_sequence_init_at(bytes, length, count_offset, &sequence) != 0) {
      continue;
    }

    cursor = sequence;
    while (mini_gnb_c_ie_sequence_next(&cursor, &ie) == 0) {
      for (id_index = 0u; id_index < required_id_count; ++id_index) {
        if (ie.id == required_ids[id_index]) {
          required_hits += 1u;
          break;
        }
      }
    }

    if (cursor.index != cursor.count || cursor.next_offset != length || required_hits < required_id_count) {
      continue;
    }

    *sequence_out = sequence;
    found = 1;
  }

  return found ? 0 : -1;
}

static int mini_gnb_c_extract_ngap_nas_pdu(const uint8_t* bytes,
                                           size_t length,
                                           mini_gnb_c_octets_t* nas_pdu_out) {
  mini_gnb_c_ngap_ie_t nas_ie;

  if (nas_pdu_out == NULL) {
    return -1;
  }
  if (mini_gnb_c_extract_ngap_ie(bytes, length, 0x0026u, &nas_ie) != 0) {
    return -1;
  }
  return mini_gnb_c_decode_octet_string(&nas_ie.value, nas_pdu_out);
}

static int mini_gnb_c_extract_pdu_session_setup_transfer_sequence(const uint8_t* bytes,
                                                                  size_t length,
                                                                  mini_gnb_c_ie_sequence_t* transfer_sequence) {
  static const uint16_t k_required_transfer_ies[] = {0x008bu, 0x0088u};
  mini_gnb_c_ngap_ie_t session_list_ie;

  if (transfer_sequence == NULL) {
    return -1;
  }
  if (mini_gnb_c_extract_ngap_ie(bytes, length, 0x004au, &session_list_ie) != 0) {
    return -1;
  }

  return mini_gnb_c_find_tail_ie_sequence(session_list_ie.value.bytes,
                                          session_list_ie.value.length,
                                          k_required_transfer_ies,
                                          sizeof(k_required_transfer_ies) / sizeof(k_required_transfer_ies[0]),
                                          transfer_sequence);
}

static int mini_gnb_c_extract_amf_ue_ngap_id(const uint8_t* bytes,
                                             size_t length,
                                             uint8_t amf_ue_ngap_id[2]) {
  mini_gnb_c_ngap_ie_t ie;

  if (amf_ue_ngap_id == NULL) {
    return -1;
  }
  if (mini_gnb_c_extract_ngap_ie(bytes, length, 0x000au, &ie) != 0 || ie.value.length != 2u) {
    return -1;
  }

  amf_ue_ngap_id[0] = ie.value.bytes[0];
  amf_ue_ngap_id[1] = ie.value.bytes[1];
  return 0;
}

static int mini_gnb_c_build_initial_ue_message(uint8_t* message,
                                               size_t message_capacity,
                                               size_t* message_length) {
  uint8_t nas_ie_value[MINI_GNB_C_NAS_MAX_LEN];
  size_t nas_ie_value_length = 0u;
  const mini_gnb_c_build_ie_t ies[] = {
      {0x0055u, 0x00u, k_ran_ue_ngap_id, sizeof(k_ran_ue_ngap_id)},
      {0x0026u, 0x00u, nas_ie_value, 0u},
      {0x0079u, 0x00u, k_user_location_information_nr, sizeof(k_user_location_information_nr)},
      {0x005au, 0x40u, k_rrc_establishment_cause_mo_signalling, sizeof(k_rrc_establishment_cause_mo_signalling)},
      {0x0070u, 0x40u, k_ue_context_request_requested, sizeof(k_ue_context_request_requested)},
  };
  mini_gnb_c_build_ie_t working_ies[sizeof(ies) / sizeof(ies[0])];

  if (message == NULL || message_length == NULL) {
    return -1;
  }
  if (mini_gnb_c_build_octet_string_value(k_initial_registration_request_nas,
                                          sizeof(k_initial_registration_request_nas),
                                          nas_ie_value,
                                          sizeof(nas_ie_value),
                                          &nas_ie_value_length) != 0) {
    return -1;
  }

  memcpy(working_ies, ies, sizeof(working_ies));
  working_ies[1].value_length = nas_ie_value_length;
  return mini_gnb_c_build_ngap_message(0x00u,
                                       0x0fu,
                                       0x40u,
                                       working_ies,
                                       sizeof(working_ies) / sizeof(working_ies[0]),
                                       message,
                                       message_capacity,
                                       message_length);
}

static int mini_gnb_c_build_uplink_nas_transport(const uint8_t amf_ue_ngap_id[2],
                                                 const uint8_t* nas_ie_value,
                                                 size_t nas_ie_value_length,
                                                 uint8_t* message,
                                                 size_t message_capacity,
                                                 size_t* message_length) {
  const mini_gnb_c_build_ie_t ies[] = {
      {0x000au, 0x00u, amf_ue_ngap_id, 2u},
      {0x0055u, 0x00u, k_ran_ue_ngap_id, sizeof(k_ran_ue_ngap_id)},
      {0x0026u, 0x00u, nas_ie_value, nas_ie_value_length},
      {0x0079u, 0x40u, k_user_location_information_nr, sizeof(k_user_location_information_nr)},
  };

  if (amf_ue_ngap_id == NULL || nas_ie_value == NULL || message == NULL || message_length == NULL) {
    return -1;
  }

  return mini_gnb_c_build_ngap_message(0x00u,
                                       0x2eu,
                                       0x40u,
                                       ies,
                                       sizeof(ies) / sizeof(ies[0]),
                                       message,
                                       message_capacity,
                                       message_length);
}

static int mini_gnb_c_build_initial_context_setup_response(const uint8_t amf_ue_ngap_id[2],
                                                           uint8_t* message,
                                                           size_t message_capacity,
                                                           size_t* message_length) {
  const mini_gnb_c_build_ie_t ies[] = {
      {0x000au, 0x40u, amf_ue_ngap_id, 2u},
      {0x0055u, 0x40u, k_ran_ue_ngap_id, sizeof(k_ran_ue_ngap_id)},
  };

  if (amf_ue_ngap_id == NULL || message == NULL || message_length == NULL) {
    return -1;
  }

  return mini_gnb_c_build_ngap_message(0x20u,
                                       0x0eu,
                                       0x00u,
                                       ies,
                                       sizeof(ies) / sizeof(ies[0]),
                                       message,
                                       message_capacity,
                                       message_length);
}

static int mini_gnb_c_build_pdu_session_resource_setup_response(const uint8_t amf_ue_ngap_id[2],
                                                                uint8_t* message,
                                                                size_t message_capacity,
                                                                size_t* message_length) {
  const mini_gnb_c_build_ie_t ies[] = {
      {0x000au, 0x40u, amf_ue_ngap_id, 2u},
      {0x0055u, 0x40u, k_ran_ue_ngap_id, sizeof(k_ran_ue_ngap_id)},
      {0x004bu, 0x40u, k_pdu_session_resource_setup_response_list,
       sizeof(k_pdu_session_resource_setup_response_list)},
  };

  if (amf_ue_ngap_id == NULL || message == NULL || message_length == NULL) {
    return -1;
  }

  return mini_gnb_c_build_ngap_message(0x20u,
                                       0x1du,
                                       0x00u,
                                       ies,
                                       sizeof(ies) / sizeof(ies[0]),
                                       message,
                                       message_capacity,
                                       message_length);
}

static int mini_gnb_c_parse_hex_string(const char* text, uint8_t* output, size_t output_len) {
  size_t text_index = 0;
  size_t out_index = 0;
  int high_nibble = -1;

  if (text == NULL || output == NULL) {
    return -1;
  }

  memset(output, 0, output_len);
  for (text_index = 0; text[text_index] != '\0'; ++text_index) {
    unsigned char ch = (unsigned char)text[text_index];
    int nibble = 0;

    if (isspace(ch)) {
      continue;
    }
    if (ch >= '0' && ch <= '9') {
      nibble = ch - '0';
    } else if (ch >= 'a' && ch <= 'f') {
      nibble = 10 + (ch - 'a');
    } else if (ch >= 'A' && ch <= 'F') {
      nibble = 10 + (ch - 'A');
    } else {
      return -1;
    }

    if (high_nibble < 0) {
      high_nibble = nibble;
    } else {
      if (out_index >= output_len) {
        return -1;
      }
      output[out_index++] = (uint8_t)((high_nibble << 4) | nibble);
      high_nibble = -1;
    }
  }

  if (high_nibble >= 0 || out_index != output_len) {
    return -1;
  }

  return 0;
}

static void mini_gnb_c_format_serving_network_name(const mini_gnb_c_probe_options_t* options,
                                                   char* buffer,
                                                   size_t buffer_len) {
  char mnc_padded[4];

  if (buffer == NULL || buffer_len == 0u || options == NULL) {
    return;
  }

  if (strlen(options->mnc) == 2u) {
    snprintf(mnc_padded, sizeof(mnc_padded), "0%s", options->mnc);
  } else {
    snprintf(mnc_padded, sizeof(mnc_padded), "%s", options->mnc);
  }

  snprintf(buffer,
           buffer_len,
           "5G:mnc%s.mcc%s.3gppnetwork.org",
           mnc_padded,
           options->mcc);
}

static const uint8_t* mini_gnb_c_find_nas_message(const uint8_t* bytes,
                                                  size_t length,
                                                  uint8_t message_type,
                                                  size_t* offset_out) {
  size_t index = 0;

  if (bytes == NULL || offset_out == NULL || length < 3u) {
    return NULL;
  }

  for (index = 0; index + 2u < length; ++index) {
    if (bytes[index] == 0x7eu && bytes[index + 1u] == 0x00u && bytes[index + 2u] == message_type) {
      *offset_out = index;
      return bytes + index;
    }
  }

  return NULL;
}

static uint8_t mini_gnb_c_extract_nas_message_type(const uint8_t* bytes, size_t length) {
  mini_gnb_c_octets_t nas_pdu;
  size_t index = 0;

  if (mini_gnb_c_extract_ngap_nas_pdu(bytes, length, &nas_pdu) == 0 && nas_pdu.length >= 3u) {
    bytes = nas_pdu.bytes;
    length = nas_pdu.length;
  }

  if (mini_gnb_c_find_nas_message(bytes, length, 0x56u, &index) != NULL) {
    return 0x56u;
  }
  if (mini_gnb_c_find_nas_message(bytes, length, 0x57u, &index) != NULL) {
    return 0x57u;
  }
  if (mini_gnb_c_find_nas_message(bytes, length, 0x58u, &index) != NULL) {
    return 0x58u;
  }
  if (mini_gnb_c_find_nas_message(bytes, length, 0x5du, &index) != NULL) {
    return 0x5du;
  }
  if (mini_gnb_c_find_nas_message(bytes, length, 0x43u, &index) != NULL) {
    return 0x43u;
  }
  return 0x00u;
}

static const char* mini_gnb_c_nas_message_name(uint8_t message_type) {
  switch (message_type) {
    case 0x43u:
      return "RegistrationComplete";
    case 0x54u:
      return "ConfigurationUpdateCommand";
    case 0x56u:
      return "AuthenticationRequest";
    case 0x57u:
      return "AuthenticationResponse";
    case 0x58u:
      return "AuthenticationReject";
    case 0x5du:
      return "SecurityModeCommand";
    default:
      return "unknown";
  }
}

static int mini_gnb_c_extract_auth_request(const uint8_t* bytes,
                                           size_t length,
                                           mini_gnb_c_aka_context_t* aka_context) {
  mini_gnb_c_octets_t nas_pdu;
  const uint8_t* nas = NULL;
  size_t nas_offset = 0u;

  if (aka_context == NULL) {
    return -1;
  }

  memset(aka_context, 0, sizeof(*aka_context));
  if (mini_gnb_c_extract_ngap_nas_pdu(bytes, length, &nas_pdu) != 0) {
    fprintf(stderr, "auth request parse: NGAP NAS-PDU IE not found\n");
    return -1;
  }

  nas = mini_gnb_c_find_nas_message(nas_pdu.bytes, nas_pdu.length, 0x56u, &nas_offset);
  if (nas == NULL || nas_offset + 5u > nas_pdu.length) {
    fprintf(stderr, "auth request parse: NAS AuthenticationRequest header not found\n");
    return -1;
  }

  /* 5GMM Authentication Request:
   * 7e 00 56 | ngKSI | abba_len | abba[...] | 21 | rand[16] | 20 10 | autn[16]
   */
  aka_context->abba_len = (size_t)nas[4];
  if (aka_context->abba_len > sizeof(aka_context->abba) || aka_context->abba_len != 2u) {
    fprintf(stderr, "auth request parse: unexpected ABBA length=%zu\n", aka_context->abba_len);
    return -1;
  }
  if (nas_offset + 5u + aka_context->abba_len + 1u + 16u + 2u + 16u > nas_pdu.length) {
    fprintf(stderr,
            "auth request parse: length check failed nas_offset=%zu total=%zu frame_len=%zu\n",
            nas_offset,
            5u + aka_context->abba_len + 1u + 16u + 2u + 16u,
            nas_pdu.length);
    return -1;
  }

  memcpy(aka_context->abba, nas + 5, aka_context->abba_len);
  if (nas[5 + aka_context->abba_len] != 0x21u) {
    fprintf(stderr, "auth request parse: missing RAND IEI got=0x%02x\n", nas[5 + aka_context->abba_len]);
    return -1;
  }
  memcpy(aka_context->rand, nas + 6 + aka_context->abba_len, sizeof(aka_context->rand));

  if (nas[22 + aka_context->abba_len] != 0x20u || nas[23 + aka_context->abba_len] != 0x10u) {
    fprintf(stderr,
            "auth request parse: unexpected AUTN IE header got=0x%02x 0x%02x\n",
            nas[22 + aka_context->abba_len],
            nas[23 + aka_context->abba_len]);
    return -1;
  }

  memcpy(aka_context->autn, nas + 24 + aka_context->abba_len, sizeof(aka_context->autn));
  return 0;
}

static int mini_gnb_c_extract_security_mode_command_algorithms(const uint8_t* bytes,
                                                               size_t length,
                                                               uint8_t* ciphering_algorithm,
                                                               uint8_t* integrity_algorithm) {
  mini_gnb_c_octets_t nas_pdu;
  size_t offset = 0u;
  const uint8_t* nas = NULL;

  if (mini_gnb_c_extract_ngap_nas_pdu(bytes, length, &nas_pdu) != 0) {
    return -1;
  }
  nas = mini_gnb_c_find_nas_message(nas_pdu.bytes, nas_pdu.length, 0x5du, &offset);

  if (nas == NULL || offset + 4u > nas_pdu.length || ciphering_algorithm == NULL || integrity_algorithm == NULL) {
    return -1;
  }

  *ciphering_algorithm = (uint8_t)((nas[3] >> 4u) & 0x0fu);
  *integrity_algorithm = (uint8_t)(nas[3] & 0x0fu);
  return 0;
}

static int mini_gnb_c_find_protected_nas_template(uint8_t* bytes,
                                                  size_t length,
                                                  uint8_t inner_message_type,
                                                  size_t* outer_offset_out,
                                                  size_t* nas_length_out) {
  size_t inner_offset = 0;
  const uint8_t* inner = NULL;

  if (bytes == NULL || outer_offset_out == NULL || nas_length_out == NULL) {
    return -1;
  }

  inner = mini_gnb_c_find_nas_message(bytes, length, inner_message_type, &inner_offset);
  if (inner == NULL || inner_offset < 7u) {
    return -1;
  }

  *outer_offset_out = inner_offset - 7u;
  if (*outer_offset_out < 1u) {
    return -1;
  }

  *nas_length_out = (size_t)bytes[*outer_offset_out - 1u];
  if (*outer_offset_out + *nas_length_out > length || *nas_length_out < 7u) {
    return -1;
  }

  return 0;
}

static int mini_gnb_c_calculate_nas_mac_nia2(const uint8_t* knas_int,
                                             uint32_t count,
                                             uint8_t direction,
                                             const uint8_t* message,
                                             size_t message_len,
                                             uint8_t mac[4]) {
  uint8_t* cmac_input = NULL;
  uint8_t cmac[16];
  uint32_t count_be = htonl(count);

  if (knas_int == NULL || message == NULL || mac == NULL || message_len == 0u) {
    return -1;
  }

  cmac_input = (uint8_t*)malloc(message_len + 8u);
  if (cmac_input == NULL) {
    return -1;
  }

  memset(cmac_input, 0, message_len + 8u);
  memcpy(cmac_input, &count_be, sizeof(count_be));
  cmac_input[4] = (uint8_t)((MINI_GNB_C_NAS_BEARER_3GPP << 3u) | (direction << 2u));
  memcpy(cmac_input + 8u, message, message_len);

  if (ogs_aes_cmac_calculate(cmac, knas_int, cmac_input, (uint32_t)(message_len + 8u)) != 0) {
    free(cmac_input);
    return -1;
  }

  memcpy(mac, cmac, 4u);
  free(cmac_input);
  return 0;
}

static int mini_gnb_c_patch_security_protected_uplink_message(uint8_t* bytes,
                                                              size_t length,
                                                              uint8_t inner_message_type,
                                                              const mini_gnb_c_aka_context_t* aka_context) {
  size_t outer_offset = 0;
  size_t nas_length = 0;
  uint8_t mac[4];
  uint8_t sequence_number = 0;
  uint32_t ul_count = 0;

  if (bytes == NULL || aka_context == NULL) {
    return -1;
  }

  if (mini_gnb_c_find_protected_nas_template(bytes,
                                             length,
                                             inner_message_type,
                                             &outer_offset,
                                             &nas_length) != 0) {
    return -1;
  }

  sequence_number = bytes[outer_offset + 6u];
  ul_count = (uint32_t)sequence_number;
  if (mini_gnb_c_calculate_nas_mac_nia2(aka_context->knas_int,
                                        ul_count,
                                        MINI_GNB_C_NAS_DIRECTION_UPLINK,
                                        bytes + outer_offset + 6u,
                                        nas_length - 6u,
                                        mac) != 0) {
    return -1;
  }

  memcpy(bytes + outer_offset + 2u, mac, sizeof(mac));
  return 0;
}

static int mini_gnb_c_extract_open5gs_upf_tunnel(const uint8_t* bytes,
                                                 size_t length,
                                                 mini_gnb_c_core_session_t* core_session) {
  mini_gnb_c_ie_sequence_t transfer_sequence;
  mini_gnb_c_ngap_ie_t gtp_tunnel_ie;
  struct in_addr upf_addr;
  char upf_ip[MINI_GNB_C_CORE_MAX_IPV4_TEXT];

  if (bytes == NULL || core_session == NULL) {
    return -1;
  }

  if (mini_gnb_c_extract_pdu_session_setup_transfer_sequence(bytes, length, &transfer_sequence) != 0 ||
      mini_gnb_c_find_ie_in_sequence(&transfer_sequence, 0x008bu, &gtp_tunnel_ie) != 0) {
    return -1;
  }
  if (gtp_tunnel_ie.value.length < 10u || gtp_tunnel_ie.value.bytes[1] != 0xf0u) {
    return -1;
  }

  memcpy(&upf_addr.s_addr, gtp_tunnel_ie.value.bytes + 2u, 4u);
  if (inet_ntop(AF_INET, &upf_addr, upf_ip, sizeof(upf_ip)) == NULL) {
    perror("inet_ntop(UPF IP)");
    return -1;
  }

  return mini_gnb_c_core_session_set_upf_tunnel(core_session,
                                                upf_ip,
                                                mini_gnb_c_read_u32_be(gtp_tunnel_ie.value.bytes + 6u));
}

static int mini_gnb_c_extract_open5gs_qfi(const uint8_t* bytes,
                                          size_t length,
                                          mini_gnb_c_core_session_t* core_session) {
  mini_gnb_c_ie_sequence_t transfer_sequence;
  mini_gnb_c_ngap_ie_t qos_flow_ie;

  if (bytes == NULL || core_session == NULL) {
    return -1;
  }
  if (mini_gnb_c_extract_pdu_session_setup_transfer_sequence(bytes, length, &transfer_sequence) != 0 ||
      mini_gnb_c_find_ie_in_sequence(&transfer_sequence, 0x0088u, &qos_flow_ie) != 0) {
    return -1;
  }
  if (qos_flow_ie.value.length < 2u || qos_flow_ie.value.bytes[1] == 0u || qos_flow_ie.value.bytes[1] > 63u) {
    return -1;
  }

  return mini_gnb_c_core_session_set_qfi(core_session, qos_flow_ie.value.bytes[1]);
}

static int mini_gnb_c_extract_open5gs_ue_ipv4(const uint8_t* bytes,
                                              size_t length,
                                              mini_gnb_c_core_session_t* core_session) {
  mini_gnb_c_ngap_ie_t session_list_ie;
  const uint8_t* nas = NULL;
  size_t nas_offset = 0u;
  size_t index = 0;

  if (bytes == NULL || core_session == NULL) {
    return -1;
  }

  if (mini_gnb_c_extract_ngap_ie(bytes, length, 0x004au, &session_list_ie) != 0) {
    return -1;
  }

  nas = mini_gnb_c_find_nas_message(session_list_ie.value.bytes,
                                    session_list_ie.value.length,
                                    0x68u,
                                    &nas_offset);
  if (nas == NULL) {
    return -1;
  }

  for (index = nas_offset; index + 6u < session_list_ie.value.length; ++index) {
    if (session_list_ie.value.bytes[index] != 0x29u || session_list_ie.value.bytes[index + 1u] != 0x05u ||
        session_list_ie.value.bytes[index + 2u] != 0x01u) {
      continue;
    }
    mini_gnb_c_core_session_set_ue_ipv4(core_session, session_list_ie.value.bytes + index + 3u);
    return 0;
  }

  return -1;
}

static uint16_t mini_gnb_c_checksum16(const uint8_t* data, size_t length) {
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

static int mini_gnb_c_ensure_parent_dir(const char* path) {
  char directory[512];
  size_t path_len = 0u;
  size_t index = 0u;

  if (path == NULL) {
    return -1;
  }

  path_len = strlen(path);
  if (path_len == 0u || path_len >= sizeof(directory)) {
    return -1;
  }

  memcpy(directory, path, path_len + 1u);
  for (index = path_len; index > 0u; --index) {
    if (directory[index] == '/') {
      directory[index] = '\0';
      break;
    }
  }
  if (index == 0u) {
    return 0;
  }

  for (index = 1u; directory[index] != '\0'; ++index) {
    if (directory[index] != '/') {
      continue;
    }
    directory[index] = '\0';
    if (mkdir(directory, 0777) != 0 && errno != EEXIST) {
      perror(directory);
      return -1;
    }
    directory[index] = '/';
  }

  if (mkdir(directory, 0777) != 0 && errno != EEXIST) {
    perror(directory);
    return -1;
  }
  return 0;
}

static int mini_gnb_c_pcap_writer_open(mini_gnb_c_pcap_writer_t* writer,
                                       const char* path,
                                       uint32_t linktype) {
  uint8_t global_header[24];

  if (writer == NULL || path == NULL) {
    return -1;
  }

  if (mini_gnb_c_ensure_parent_dir(path) != 0) {
    return -1;
  }

  memset(writer, 0, sizeof(*writer));
  writer->file = fopen(path, "wb");
  if (writer->file == NULL) {
    perror(path);
    return -1;
  }
  writer->linktype = linktype;

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
    perror("fwrite(pcap global header)");
    fclose(writer->file);
    writer->file = NULL;
    return -1;
  }

  return 0;
}

static void mini_gnb_c_pcap_writer_close(mini_gnb_c_pcap_writer_t* writer) {
  if (writer == NULL || writer->file == NULL) {
    return;
  }

  fclose(writer->file);
  writer->file = NULL;
  writer->linktype = 0u;
}

static int mini_gnb_c_pcap_writer_write(mini_gnb_c_pcap_writer_t* writer,
                                        const uint8_t* bytes,
                                        size_t length) {
  struct timeval now;
  uint8_t record_header[16];
  uint32_t ts_sec = 0u;
  uint32_t ts_usec = 0u;
  uint32_t captured_length = 0u;

  if (writer == NULL || writer->file == NULL || bytes == NULL || length == 0u || length > 0xffffffffu) {
    return -1;
  }

  if (gettimeofday(&now, NULL) != 0) {
    perror("gettimeofday");
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
    perror("fwrite(pcap packet)");
    return -1;
  }

  fflush(writer->file);
  return 0;
}

static void mini_gnb_c_trace_close(void) {
  mini_gnb_c_pcap_writer_close(&g_mini_gnb_c_ngap_trace_writer);
  mini_gnb_c_pcap_writer_close(&g_mini_gnb_c_gtpu_trace_writer);
}

static void mini_gnb_c_trace_init(const mini_gnb_c_probe_options_t* options) {
  if (options == NULL) {
    return;
  }

  memset(&g_mini_gnb_c_ngap_trace_writer, 0, sizeof(g_mini_gnb_c_ngap_trace_writer));
  memset(&g_mini_gnb_c_gtpu_trace_writer, 0, sizeof(g_mini_gnb_c_gtpu_trace_writer));

  if (options->ngap_trace_pcap_path != NULL &&
      mini_gnb_c_pcap_writer_open(&g_mini_gnb_c_ngap_trace_writer,
                                  options->ngap_trace_pcap_path,
                                  MINI_GNB_C_PCAP_LINKTYPE_USER5) != 0) {
    fprintf(stderr, "warning: failed to open NGAP trace pcap %s\n", options->ngap_trace_pcap_path);
  }

  if (options->gtpu_trace_pcap_path != NULL &&
      mini_gnb_c_pcap_writer_open(&g_mini_gnb_c_gtpu_trace_writer,
                                  options->gtpu_trace_pcap_path,
                                  MINI_GNB_C_PCAP_LINKTYPE_RAW) != 0) {
    fprintf(stderr, "warning: failed to open GTP-U trace pcap %s\n", options->gtpu_trace_pcap_path);
  }
}

static void mini_gnb_c_trace_ngap_packet(const uint8_t* bytes, size_t length) {
  if (g_mini_gnb_c_ngap_trace_writer.file == NULL) {
    return;
  }

  if (mini_gnb_c_pcap_writer_write(&g_mini_gnb_c_ngap_trace_writer, bytes, length) != 0) {
    fprintf(stderr, "warning: failed to append NGAP trace packet\n");
  }
}

static int mini_gnb_c_build_outer_ipv4_udp_packet(const struct sockaddr_in* src_addr,
                                                  const struct sockaddr_in* dst_addr,
                                                  const uint8_t* udp_payload,
                                                  size_t udp_payload_length,
                                                  uint8_t* packet,
                                                  size_t packet_capacity,
                                                  size_t* packet_length) {
  uint16_t total_length = 0u;
  uint16_t udp_length = 0u;

  if (src_addr == NULL || dst_addr == NULL || udp_payload == NULL || packet == NULL || packet_length == NULL) {
    return -1;
  }

  total_length = (uint16_t)(20u + 8u + udp_payload_length);
  udp_length = (uint16_t)(8u + udp_payload_length);
  if ((size_t)total_length > packet_capacity) {
    return -1;
  }

  memset(packet, 0, total_length);
  packet[0] = 0x45u;
  packet[1] = 0x00u;
  packet[2] = (uint8_t)(total_length >> 8u);
  packet[3] = (uint8_t)(total_length & 0xffu);
  packet[4] = 0x43u;
  packet[5] = 0x21u;
  packet[6] = 0x00u;
  packet[7] = 0x00u;
  packet[8] = 64u;
  packet[9] = 17u;
  memcpy(packet + 12u, &src_addr->sin_addr.s_addr, 4u);
  memcpy(packet + 16u, &dst_addr->sin_addr.s_addr, 4u);
  {
    uint16_t ip_checksum = mini_gnb_c_checksum16(packet, 20u);
    packet[10] = (uint8_t)(ip_checksum >> 8u);
    packet[11] = (uint8_t)(ip_checksum & 0xffu);
  }

  memcpy(packet + 20u, &src_addr->sin_port, 2u);
  memcpy(packet + 22u, &dst_addr->sin_port, 2u);
  packet[24] = (uint8_t)(udp_length >> 8u);
  packet[25] = (uint8_t)(udp_length & 0xffu);
  packet[26] = 0x00u;
  packet[27] = 0x00u;
  memcpy(packet + 28u, udp_payload, udp_payload_length);
  *packet_length = total_length;
  return 0;
}

static void mini_gnb_c_trace_gtpu_packet(const struct sockaddr_in* src_addr,
                                         const struct sockaddr_in* dst_addr,
                                         const uint8_t* udp_payload,
                                         size_t udp_payload_length) {
  uint8_t packet[2048];
  size_t packet_length = 0u;
  struct sockaddr_in normalized_src;
  struct sockaddr_in normalized_dst;

  if (g_mini_gnb_c_gtpu_trace_writer.file == NULL) {
    return;
  }

  normalized_src = *src_addr;
  normalized_dst = *dst_addr;
  if (normalized_src.sin_addr.s_addr == htonl(INADDR_ANY) &&
      ((ntohl(normalized_dst.sin_addr.s_addr) >> 24u) & 0xffu) == 127u) {
    (void)inet_pton(AF_INET, "127.0.0.1", &normalized_src.sin_addr);
  }
  if (normalized_dst.sin_addr.s_addr == htonl(INADDR_ANY) &&
      ((ntohl(normalized_src.sin_addr.s_addr) >> 24u) & 0xffu) == 127u) {
    (void)inet_pton(AF_INET, "127.0.0.1", &normalized_dst.sin_addr);
  }

  if (mini_gnb_c_build_outer_ipv4_udp_packet(&normalized_src,
                                             &normalized_dst,
                                             udp_payload,
                                             udp_payload_length,
                                             packet,
                                             sizeof(packet),
                                             &packet_length) != 0) {
    fprintf(stderr, "warning: failed to build outer IPv4/UDP packet for GTP-U trace\n");
    return;
  }
  if (mini_gnb_c_pcap_writer_write(&g_mini_gnb_c_gtpu_trace_writer, packet, packet_length) != 0) {
    fprintf(stderr, "warning: failed to append GTP-U trace packet\n");
  }
}

static void mini_gnb_c_trace_gtpu_tx(int socket_fd,
                                     const struct sockaddr_in* dst_addr,
                                     const uint8_t* udp_payload,
                                     size_t udp_payload_length) {
  struct sockaddr_in src_addr;
  socklen_t src_addr_len = (socklen_t)sizeof(src_addr);

  memset(&src_addr, 0, sizeof(src_addr));
  if (socket_fd < 0 || dst_addr == NULL || udp_payload == NULL) {
    return;
  }
  if (getsockname(socket_fd, (struct sockaddr*)&src_addr, &src_addr_len) != 0) {
    perror("getsockname(GTP-U tx)");
    return;
  }

  mini_gnb_c_trace_gtpu_packet(&src_addr, dst_addr, udp_payload, udp_payload_length);
}

static void mini_gnb_c_trace_gtpu_rx(int socket_fd,
                                     const struct sockaddr_in* src_addr,
                                     const uint8_t* udp_payload,
                                     size_t udp_payload_length) {
  struct sockaddr_in dst_addr;
  socklen_t dst_addr_len = (socklen_t)sizeof(dst_addr);

  memset(&dst_addr, 0, sizeof(dst_addr));
  if (socket_fd < 0 || src_addr == NULL || udp_payload == NULL) {
    return;
  }
  if (getsockname(socket_fd, (struct sockaddr*)&dst_addr, &dst_addr_len) != 0) {
    perror("getsockname(GTP-U rx)");
    return;
  }

  mini_gnb_c_trace_gtpu_packet(src_addr, &dst_addr, udp_payload, udp_payload_length);
}

static int mini_gnb_c_send_gtpu_gpdu(const mini_gnb_c_probe_options_t* options,
                                     const mini_gnb_c_core_session_t* core_session) {
  int socket_fd = -1;
  struct sockaddr_in upf_addr;
  uint8_t inner_packet[256];
  size_t inner_packet_length = 0u;
  uint8_t gtpu_packet[512];
  size_t gtpu_packet_length = 0u;
  const char* upf_ip = NULL;

  if (options == NULL || core_session == NULL) {
    return -1;
  }
  if (!core_session->upf_tunnel_valid || !core_session->ue_ipv4_valid) {
    fprintf(stderr, "missing parsed UPF tunnel or UE IPv4 for G-PDU probe\n");
    return -1;
  }
  if (!core_session->qfi_valid) {
    fprintf(stderr, "missing parsed QFI for G-PDU probe\n");
    return -1;
  }

  upf_ip = core_session->upf_ip[0] != '\0' ? core_session->upf_ip : options->upf_ip;
  if (mini_gnb_c_gtpu_build_ipv4_udp_probe(core_session,
                                           options->gpdu_dst_ip,
                                           inner_packet,
                                           sizeof(inner_packet),
                                           &inner_packet_length) != 0) {
    return -1;
  }
  if (mini_gnb_c_gtpu_build_gpdu(core_session,
                                 inner_packet,
                                 inner_packet_length,
                                 gtpu_packet,
                                 sizeof(gtpu_packet),
                                 &gtpu_packet_length) != 0) {
    return -1;
  }

  memset(&upf_addr, 0, sizeof(upf_addr));
  upf_addr.sin_family = AF_INET;
  upf_addr.sin_port = htons((uint16_t)options->upf_port);
  if (inet_pton(AF_INET, upf_ip, &upf_addr.sin_addr) != 1) {
    fprintf(stderr, "invalid UPF IP for G-PDU probe: %s\n", upf_ip);
    return -1;
  }

  socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (socket_fd < 0) {
    perror("socket(GTP-U G-PDU)");
    return -1;
  }

  if (mini_gnb_c_set_timeouts(socket_fd, options->timeout_ms) != 0) {
    close(socket_fd);
    return -1;
  }
  if (connect(socket_fd, (const struct sockaddr*)&upf_addr, (socklen_t)sizeof(upf_addr)) != 0) {
    perror("connect(GTP-U G-PDU)");
    close(socket_fd);
    return -1;
  }

  printf("Sending GTP-U G-PDU to parsed UPF tunnel %s:%u TEID=0x%08x QFI=%u UE=%u.%u.%u.%u dst=%s...\n",
         upf_ip,
         options->upf_port,
         core_session->upf_teid,
         core_session->qfi,
         core_session->ue_ipv4[0],
         core_session->ue_ipv4[1],
         core_session->ue_ipv4[2],
         core_session->ue_ipv4[3],
         options->gpdu_dst_ip);
  if (send(socket_fd, gtpu_packet, gtpu_packet_length, 0) != (ssize_t)gtpu_packet_length) {
    perror("send(GTP-U G-PDU)");
    close(socket_fd);
    return -1;
  }
  mini_gnb_c_trace_gtpu_tx(socket_fd, &upf_addr, gtpu_packet, gtpu_packet_length);

  close(socket_fd);
  return 0;
}

static int mini_gnb_c_build_auth_response_nas(const mini_gnb_c_probe_options_t* options,
                                              const mini_gnb_c_aka_context_t* auth_request,
                                              uint8_t* nas_out,
                                              size_t* nas_out_len,
                                              mini_gnb_c_aka_context_t* auth_result) {
  uint8_t key[16];
  uint8_t opc[16];
  uint8_t auth_amf[2];
  char serving_network_name[64];
  mini_gnb_c_aka_context_t working;

  if (options == NULL || auth_request == NULL || nas_out == NULL || nas_out_len == NULL || auth_result == NULL) {
    return -1;
  }

  if (mini_gnb_c_parse_hex_string(options->key_hex, key, sizeof(key)) != 0 ||
      mini_gnb_c_parse_hex_string(options->opc_hex, opc, sizeof(opc)) != 0 ||
      mini_gnb_c_parse_hex_string(options->auth_amf_hex, auth_amf, sizeof(auth_amf)) != 0) {
    fprintf(stderr, "failed to parse AKA material from options\n");
    return -1;
  }

  memcpy(&working, auth_request, sizeof(working));
  working.res_len = 8u;
  working.res_star_len = 16u;
  working.ciphering_algorithm = MINI_GNB_C_NAS_ALG_NEA0;
  working.integrity_algorithm = MINI_GNB_C_NAS_ALG_NIA2;
  mini_gnb_c_format_serving_network_name(options, serving_network_name, sizeof(serving_network_name));

  if (milenage_f2345(opc,
                     key,
                     working.rand,
                     working.res,
                     working.ck,
                     working.ik,
                     working.ak,
                     NULL) != 0) {
    fprintf(stderr, "milenage_f2345 failed\n");
    return -1;
  }

  memcpy(working.sqn_xor_ak, working.autn, sizeof(working.sqn_xor_ak));
  for (size_t i = 0; i < sizeof(working.sqn); ++i) {
    working.sqn[i] = working.autn[i] ^ working.ak[i];
  }

  ogs_kdf_xres_star(working.ck,
                    working.ik,
                    serving_network_name,
                    working.rand,
                    working.res,
                    working.res_len,
                    working.res_star);
  {
    char supi[32];

    snprintf(supi, sizeof(supi), "imsi-%s", options->imsi);
    ogs_kdf_kausf(working.ck, working.ik, serving_network_name, working.autn, working.kausf);
    ogs_kdf_kseaf(serving_network_name, working.kausf, working.kseaf);
    ogs_kdf_kamf(supi,
                 working.abba,
                 (uint8_t)working.abba_len,
                 working.kseaf,
                 working.kamf);
  }
  ogs_kdf_nas_5gs(MINI_GNB_C_KDF_NAS_ENC_ALG,
                  working.ciphering_algorithm,
                  working.kamf,
                  working.knas_enc);
  ogs_kdf_nas_5gs(MINI_GNB_C_KDF_NAS_INT_ALG,
                  working.integrity_algorithm,
                  working.kamf,
                  working.knas_int);

  /* Plain NAS AuthenticationResponse with RES*. */
  nas_out[0] = 0x7eu;
  nas_out[1] = 0x00u;
  nas_out[2] = 0x57u;
  nas_out[3] = 0x2du;
  nas_out[4] = 0x10u;
  memcpy(nas_out + 5, working.res_star, 16u);
  *nas_out_len = 21u;

  memcpy(auth_result, &working, sizeof(working));
  (void)auth_amf;
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

static int mini_gnb_c_send_ngap(int socket_fd,
                                const char* socket_name,
                                const char* label,
                                const uint8_t* bytes,
                                size_t length) {
  ssize_t bytes_sent = 0;
  int send_flags = 0;

  if (socket_fd < 0 || bytes == NULL || length == 0u) {
    return -1;
  }

#ifdef MSG_NOSIGNAL
  send_flags = MSG_NOSIGNAL;
#endif

  printf("Sending %s (%zu bytes) over %s...\n", label, length, socket_name);
  bytes_sent = send(socket_fd, bytes, length, send_flags);
  if (bytes_sent < 0) {
    perror("send");
    return -1;
  }
  if ((size_t)bytes_sent != length) {
    fprintf(stderr, "short send: expected %zu, got %zd\n", length, bytes_sent);
    return -1;
  }

  mini_gnb_c_trace_ngap_packet(bytes, length);
  return 0;
}

static int mini_gnb_c_recv_ngap(int socket_fd,
                                uint8_t* response,
                                size_t response_capacity,
                                size_t* response_length) {
  ssize_t bytes_received = 0;

  if (socket_fd < 0 || response == NULL || response_capacity == 0u || response_length == NULL) {
    return -1;
  }

  bytes_received = recv(socket_fd, response, response_capacity, 0);
  if (bytes_received < 0) {
    perror("recv");
    return -1;
  }

  *response_length = (size_t)bytes_received;
  mini_gnb_c_trace_ngap_packet(response, *response_length);
  return 0;
}

static int mini_gnb_c_run_gtpu_echo_probe(const mini_gnb_c_probe_options_t* options,
                                          const mini_gnb_c_core_session_t* core_session) {
  int socket_fd = -1;
  struct sockaddr_in upf_addr;
  uint8_t request[14];
  size_t request_length = 0u;
  uint8_t response[256];
  struct sockaddr_in from_addr;
  socklen_t from_addr_len = (socklen_t)sizeof(from_addr);
  ssize_t bytes_received = 0;
  uint16_t sequence_number = 1u;
  const char* upf_ip = NULL;

  if (options == NULL) {
    return -1;
  }

  upf_ip = options->upf_ip;
  if (core_session != NULL && core_session->upf_tunnel_valid && core_session->upf_ip[0] != '\0') {
    upf_ip = core_session->upf_ip;
  }

  memset(&upf_addr, 0, sizeof(upf_addr));
  upf_addr.sin_family = AF_INET;
  upf_addr.sin_port = htons((uint16_t)options->upf_port);
  if (inet_pton(AF_INET, upf_ip, &upf_addr.sin_addr) != 1) {
    fprintf(stderr, "invalid upf_ip: %s\n", upf_ip);
    return -1;
  }

  socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (socket_fd < 0) {
    perror("socket(UDP)");
    return -1;
  }

  if (mini_gnb_c_set_timeouts(socket_fd, options->timeout_ms) != 0) {
    close(socket_fd);
    return -1;
  }
  if (connect(socket_fd, (const struct sockaddr*)&upf_addr, (socklen_t)sizeof(upf_addr)) != 0) {
    perror("connect(GTP-U Echo)");
    close(socket_fd);
    return -1;
  }

  if (mini_gnb_c_gtpu_build_echo_request(sequence_number, request, sizeof(request), &request_length) != 0) {
    close(socket_fd);
    return -1;
  }

  printf("Sending GTP-U Echo Request to UPF %s:%u...\n", upf_ip, options->upf_port);
  if (send(socket_fd, request, request_length, 0) != (ssize_t)request_length) {
    perror("send(GTP-U Echo Request)");
    close(socket_fd);
    return -1;
  }
  mini_gnb_c_trace_gtpu_tx(socket_fd, &upf_addr, request, request_length);

  bytes_received = recvfrom(socket_fd,
                            response,
                            sizeof(response),
                            0,
                            (struct sockaddr*)&from_addr,
                            &from_addr_len);
  if (bytes_received < 0) {
    perror("recvfrom(GTP-U Echo Response)");
    close(socket_fd);
    return -1;
  }
  mini_gnb_c_trace_gtpu_rx(socket_fd, &from_addr, response, (size_t)bytes_received);

  printf("Received %zd bytes from UPF %s:%u\n",
         bytes_received,
         upf_ip,
         options->upf_port);
  printf("GTP-U response hex:\n");
  mini_gnb_c_print_hex(response, (size_t)bytes_received);

  if (mini_gnb_c_gtpu_validate_echo_response(response, (size_t)bytes_received, sequence_number) != 0) {
    fprintf(stderr, "unexpected GTP-U Echo Response content\n");
    close(socket_fd);
    return -1;
  }

  printf("GTP-U Echo Response detected.\n");
  close(socket_fd);
  return 0;
}

static int mini_gnb_c_print_received_ngap(const uint8_t* response, size_t response_length) {
  if (response == NULL || response_length < 2u) {
    fprintf(stderr, "received NGAP PDU too short\n");
    return -1;
  }

  printf("Received %zu bytes from AMF. pdu=%s(0x%02x) procedure=%s(0x%02x)\n",
         response_length,
         mini_gnb_c_pdu_type_name(response[0]),
         response[0],
         mini_gnb_c_procedure_name(response[1]),
         response[1]);
  printf("Response hex:\n");
  mini_gnb_c_print_hex(response, response_length);
  return 0;
}

static int mini_gnb_c_expect_ngap(const uint8_t* response,
                                  size_t response_length,
                                  uint8_t expected_pdu_type,
                                  uint8_t expected_proc_code,
                                  const char* context_label) {
  if (response == NULL || response_length < 2u) {
    fprintf(stderr, "%s: response too short\n", context_label);
    return -1;
  }

  if (response[0] != expected_pdu_type || response[1] != expected_proc_code) {
    fprintf(stderr,
            "%s: unexpected NGAP response. expected pdu=%s(0x%02x) procedure=%s(0x%02x), "
            "got pdu=%s(0x%02x) procedure=%s(0x%02x)\n",
            context_label,
            mini_gnb_c_pdu_type_name(expected_pdu_type),
            expected_pdu_type,
            mini_gnb_c_procedure_name(expected_proc_code),
            expected_proc_code,
            mini_gnb_c_pdu_type_name(response[0]),
            response[0],
            mini_gnb_c_procedure_name(response[1]),
            response[1]);
    return -1;
  }

  return 0;
}

static int mini_gnb_c_load_pcap_frames(const char* path,
                                       mini_gnb_c_pcap_frame_t* frames,
                                       size_t frame_capacity,
                                       size_t* frame_count) {
  FILE* file = NULL;
  uint8_t global_header[24];
  size_t count = 0;
  uint8_t packet_header[16];

  if (path == NULL || frames == NULL || frame_count == NULL) {
    return -1;
  }

  memset(frames, 0, sizeof(*frames) * frame_capacity);
  *frame_count = 0u;

  file = fopen(path, "rb");
  if (file == NULL) {
    perror(path);
    return -1;
  }

  if (fread(global_header, 1u, sizeof(global_header), file) != sizeof(global_header)) {
    fprintf(stderr, "failed to read pcap global header from %s\n", path);
    fclose(file);
    return -1;
  }

  if (mini_gnb_c_read_u32_le(global_header) != 0xa1b2c3d4u) {
    fprintf(stderr, "unsupported pcap magic in %s\n", path);
    fclose(file);
    return -1;
  }

  while (count < frame_capacity && fread(packet_header, 1u, sizeof(packet_header), file) == sizeof(packet_header)) {
    uint32_t incl_len = mini_gnb_c_read_u32_le(packet_header + 8u);
    uint8_t* data = NULL;

    data = (uint8_t*)malloc((size_t)incl_len);
    if (data == NULL) {
      fprintf(stderr, "out of memory while loading %s\n", path);
      fclose(file);
      return -1;
    }

    if (fread(data, 1u, (size_t)incl_len, file) != (size_t)incl_len) {
      fprintf(stderr, "truncated packet while reading %s\n", path);
      free(data);
      fclose(file);
      return -1;
    }

    frames[count].data = data;
    frames[count].length = (size_t)incl_len;
    ++count;
  }

  fclose(file);
  *frame_count = count;
  return 0;
}

static void mini_gnb_c_free_pcap_frames(mini_gnb_c_pcap_frame_t* frames, size_t frame_count) {
  size_t index = 0;

  if (frames == NULL) {
    return;
  }

  for (index = 0; index < frame_count; ++index) {
    free(frames[index].data);
    frames[index].data = NULL;
    frames[index].length = 0u;
  }
}

static const mini_gnb_c_pcap_frame_t* mini_gnb_c_get_frame(const mini_gnb_c_pcap_frame_t* frames,
                                                           size_t frame_count,
                                                           uint32_t frame_number) {
  if (frames == NULL || frame_number == 0u || (size_t)frame_number > frame_count) {
    return NULL;
  }

  return &frames[frame_number - 1u];
}

static int mini_gnb_c_probe_socket_type(const char* socket_name,
                                        int socket_type,
                                        const mini_gnb_c_probe_options_t* options) {
  int socket_fd = -1;
  struct sockaddr_in amf_addr;
  uint8_t response[4096];
  size_t response_length = 0u;

  if (options == NULL) {
    return 1;
  }

  memset(&amf_addr, 0, sizeof(amf_addr));
  amf_addr.sin_family = AF_INET;
  amf_addr.sin_port = htons((uint16_t)options->amf_port);
  if (inet_pton(AF_INET, options->amf_ip, &amf_addr.sin_addr) != 1) {
    fprintf(stderr, "invalid amf_ip: %s\n", options->amf_ip);
    return 1;
  }

  socket_fd = socket(AF_INET, socket_type, IPPROTO_SCTP);
  if (socket_fd < 0) {
    perror("socket(SCTP)");
    return 1;
  }

  if (mini_gnb_c_set_timeouts(socket_fd, options->timeout_ms) != 0) {
    close(socket_fd);
    return 1;
  }

  if (mini_gnb_c_set_ngap_ppid(socket_fd) != 0) {
    close(socket_fd);
    return 1;
  }

  printf("Connecting to AMF %s:%u via SCTP %s...\n", options->amf_ip, options->amf_port, socket_name);
  if (mini_gnb_c_connect_with_timeout(socket_fd,
                                      (const struct sockaddr*)&amf_addr,
                                      sizeof(amf_addr),
                                      options->timeout_ms) != 0) {
    close(socket_fd);
    return 1;
  }

  if (mini_gnb_c_send_ngap(socket_fd,
                           socket_name,
                           "NGSetupRequest",
                           k_ngsetup_request,
                           sizeof(k_ngsetup_request)) != 0) {
    close(socket_fd);
    return 1;
  }

  if (mini_gnb_c_recv_ngap(socket_fd, response, sizeof(response), &response_length) != 0) {
    close(socket_fd);
    return 1;
  }

  if (mini_gnb_c_print_received_ngap(response, response_length) != 0) {
    close(socket_fd);
    return 1;
  }

  if (!mini_gnb_c_is_ngsetup_response(response, response_length)) {
    fprintf(stderr, "response is not recognized as NGSetupResponse\n");
    close(socket_fd);
    return 2;
  }

  printf("NGSetupResponse detected.\n");
  close(socket_fd);
  return 0;
}

static int mini_gnb_c_replay_socket_type(const char* socket_name,
                                         int socket_type,
                                         const mini_gnb_c_probe_options_t* options) {
  int socket_fd = -1;
  struct sockaddr_in amf_addr;
  mini_gnb_c_pcap_frame_t frames[MINI_GNB_C_MAX_PCAP_FRAMES];
  size_t frame_count = 0u;
  int reference_pcap_loaded = 0;
  uint8_t response[4096];
  size_t response_length = 0u;
  size_t step_index = 0u;
  uint8_t tx_message[4096];
  uint8_t runtime_ue_token[2] = {0x01u, 0x00u};
  int runtime_ue_token_valid = 0;
  mini_gnb_c_aka_context_t aka_request;
  mini_gnb_c_aka_context_t aka_result;
  mini_gnb_c_core_session_t core_session;
  int aka_request_valid = 0;

  if (options == NULL) {
    return 1;
  }

  memset(frames, 0, sizeof(frames));
  mini_gnb_c_core_session_reset(&core_session);

  if (options->replay_pcap_path != NULL && options->replay_pcap_path[0] != '\0') {
    struct stat replay_pcap_stat;

    if (stat(options->replay_pcap_path, &replay_pcap_stat) == 0) {
      if (mini_gnb_c_load_pcap_frames(options->replay_pcap_path,
                                      frames,
                                      MINI_GNB_C_MAX_PCAP_FRAMES,
                                      &frame_count) == 0) {
        reference_pcap_loaded = 1;
      } else {
        fprintf(stderr,
                "warning: failed to load reference replay pcap %s; "
                "continuing with built-in dynamic replay steps\n",
                options->replay_pcap_path);
      }
    } else {
      fprintf(stderr,
              "warning: reference replay pcap not found at %s; "
              "continuing with built-in dynamic replay steps\n",
              options->replay_pcap_path);
    }
  }

  memset(&amf_addr, 0, sizeof(amf_addr));
  amf_addr.sin_family = AF_INET;
  amf_addr.sin_port = htons((uint16_t)options->amf_port);
  if (inet_pton(AF_INET, options->amf_ip, &amf_addr.sin_addr) != 1) {
    fprintf(stderr, "invalid amf_ip: %s\n", options->amf_ip);
    mini_gnb_c_free_pcap_frames(frames, frame_count);
    return 1;
  }

  socket_fd = socket(AF_INET, socket_type, IPPROTO_SCTP);
  if (socket_fd < 0) {
    perror("socket(SCTP)");
    mini_gnb_c_free_pcap_frames(frames, frame_count);
    return 1;
  }

  if (mini_gnb_c_set_timeouts(socket_fd, options->timeout_ms) != 0) {
    close(socket_fd);
    mini_gnb_c_free_pcap_frames(frames, frame_count);
    return 1;
  }

  if (mini_gnb_c_set_ngap_ppid(socket_fd) != 0) {
    close(socket_fd);
    mini_gnb_c_free_pcap_frames(frames, frame_count);
    return 1;
  }

  printf("Connecting to AMF %s:%u via SCTP %s for replay mode...\n",
         options->amf_ip,
         options->amf_port,
         socket_name);
  if (mini_gnb_c_connect_with_timeout(socket_fd,
                                      (const struct sockaddr*)&amf_addr,
                                      sizeof(amf_addr),
                                      options->timeout_ms) != 0) {
    close(socket_fd);
    mini_gnb_c_free_pcap_frames(frames, frame_count);
    return 1;
  }

  if (reference_pcap_loaded) {
    printf("Loaded %zu reference frames from %s\n", frame_count, options->replay_pcap_path);
  }
  for (step_index = 0u; step_index < sizeof(k_replay_steps) / sizeof(k_replay_steps[0]); ++step_index) {
    const mini_gnb_c_replay_step_t* step = &k_replay_steps[step_index];
    size_t tx_message_length = 0u;

    if (reference_pcap_loaded) {
      const mini_gnb_c_pcap_frame_t* frame = mini_gnb_c_get_frame(frames, frame_count, step->frame_number);

      if (frame == NULL || frame->data == NULL || frame->length == 0u) {
        fprintf(stderr,
                "warning: missing reference replay frame %u in %s; "
                "continuing with built-in dynamic message construction\n",
                step->frame_number,
                options->replay_pcap_path);
      }
    }
    if (step->frame_number > 3u && !runtime_ue_token_valid) {
      fprintf(stderr, "missing runtime AMF UE token before replay frame %u\n", step->frame_number);
      close(socket_fd);
      mini_gnb_c_free_pcap_frames(frames, frame_count);
      return 1;
    }

    switch (step->frame_number) {
      case 1u:
        memcpy(tx_message, k_ngsetup_request, sizeof(k_ngsetup_request));
        tx_message_length = sizeof(k_ngsetup_request);
        break;
      case 3u:
        if (mini_gnb_c_build_initial_ue_message(tx_message,
                                                sizeof(tx_message),
                                                &tx_message_length) != 0) {
          fprintf(stderr, "failed to build InitialUEMessage\n");
          close(socket_fd);
          mini_gnb_c_free_pcap_frames(frames, frame_count);
          return 1;
        }
        break;
      case 5u: {
        uint8_t nas_ie_value[MINI_GNB_C_NAS_MAX_LEN];
        size_t nas_ie_value_length = 0u;

        if (mini_gnb_c_build_octet_string_value(k_identity_response_nas,
                                                sizeof(k_identity_response_nas),
                                                nas_ie_value,
                                                sizeof(nas_ie_value),
                                                &nas_ie_value_length) != 0 ||
            mini_gnb_c_build_uplink_nas_transport(runtime_ue_token,
                                                  nas_ie_value,
                                                  nas_ie_value_length,
                                                  tx_message,
                                                  sizeof(tx_message),
                                                  &tx_message_length) != 0) {
          fprintf(stderr, "failed to build IdentityResponse UplinkNASTransport\n");
          close(socket_fd);
          mini_gnb_c_free_pcap_frames(frames, frame_count);
          return 1;
        }
        break;
      }
      case 7u: {
        uint8_t auth_response_nas[MINI_GNB_C_NAS_MAX_LEN];
        size_t auth_response_nas_len = 0u;
        uint8_t nas_ie_value[MINI_GNB_C_NAS_MAX_LEN];
        size_t nas_ie_value_length = 0u;

        if (!aka_request_valid) {
          fprintf(stderr, "missing runtime AuthenticationRequest before frame 7 replay\n");
          close(socket_fd);
          mini_gnb_c_free_pcap_frames(frames, frame_count);
          return 1;
        }
        if (mini_gnb_c_build_auth_response_nas(options,
                                               &aka_request,
                                               auth_response_nas,
                                               &auth_response_nas_len,
                                               &aka_result) != 0 ||
            mini_gnb_c_build_octet_string_value(auth_response_nas,
                                                auth_response_nas_len,
                                                nas_ie_value,
                                                sizeof(nas_ie_value),
                                                &nas_ie_value_length) != 0 ||
            mini_gnb_c_build_uplink_nas_transport(runtime_ue_token,
                                                  nas_ie_value,
                                                  nas_ie_value_length,
                                                  tx_message,
                                                  sizeof(tx_message),
                                                  &tx_message_length) != 0) {
          fprintf(stderr, "failed to build AuthenticationResponse UplinkNASTransport\n");
          close(socket_fd);
          mini_gnb_c_free_pcap_frames(frames, frame_count);
          return 1;
        }
        printf("Built AuthenticationResponse with runtime RES* derived from RAND/AUTN.\n");
        break;
      }
      case 9u:
      case 12u:
      case 14u: {
        const uint8_t* nas_template = NULL;
        size_t nas_template_length = 0u;
        uint8_t inner_message_type = 0u;
        uint8_t nas_ie_value[MINI_GNB_C_NAS_MAX_LEN];
        size_t nas_ie_value_length = 0u;

        if (step->frame_number == 9u) {
          nas_template = k_security_mode_complete_nas_template;
          nas_template_length = sizeof(k_security_mode_complete_nas_template);
          inner_message_type = 0x5eu;
        } else if (step->frame_number == 12u) {
          nas_template = k_registration_complete_nas_template;
          nas_template_length = sizeof(k_registration_complete_nas_template);
          inner_message_type = 0x43u;
        } else {
          nas_template = k_pdu_session_establishment_request_nas_template;
          nas_template_length = sizeof(k_pdu_session_establishment_request_nas_template);
          inner_message_type = 0x67u;
        }

        if (mini_gnb_c_build_octet_string_value(nas_template,
                                                nas_template_length,
                                                nas_ie_value,
                                                sizeof(nas_ie_value),
                                                &nas_ie_value_length) != 0 ||
            mini_gnb_c_patch_security_protected_uplink_message(nas_ie_value,
                                                               nas_ie_value_length,
                                                               inner_message_type,
                                                               &aka_result) != 0 ||
            mini_gnb_c_build_uplink_nas_transport(runtime_ue_token,
                                                  nas_ie_value,
                                                  nas_ie_value_length,
                                                  tx_message,
                                                  sizeof(tx_message),
                                                  &tx_message_length) != 0) {
          fprintf(stderr, "failed to build security-protected UplinkNASTransport for replay frame %u\n",
                  step->frame_number);
          close(socket_fd);
          mini_gnb_c_free_pcap_frames(frames, frame_count);
          return 1;
        }
        printf("Built security-protected UplinkNASTransport for replay frame %u.\n", step->frame_number);
        break;
      }
      case 11u:
        if (mini_gnb_c_build_initial_context_setup_response(runtime_ue_token,
                                                            tx_message,
                                                            sizeof(tx_message),
                                                            &tx_message_length) != 0) {
          fprintf(stderr, "failed to build InitialContextSetupResponse\n");
          close(socket_fd);
          mini_gnb_c_free_pcap_frames(frames, frame_count);
          return 1;
        }
        break;
      case 16u:
        if (mini_gnb_c_build_pdu_session_resource_setup_response(runtime_ue_token,
                                                                 tx_message,
                                                                 sizeof(tx_message),
                                                                 &tx_message_length) != 0) {
          fprintf(stderr, "failed to build PDUSessionResourceSetupResponse\n");
          close(socket_fd);
          mini_gnb_c_free_pcap_frames(frames, frame_count);
          return 1;
        }
        break;
      default:
        fprintf(stderr, "unsupported replay frame %u\n", step->frame_number);
        close(socket_fd);
        mini_gnb_c_free_pcap_frames(frames, frame_count);
        return 1;
    }

    if (reference_pcap_loaded) {
      printf("Replay step %zu/%zu aligned with reference pcap frame %u: %s\n",
             step_index + 1u,
             sizeof(k_replay_steps) / sizeof(k_replay_steps[0]),
             step->frame_number,
             step->label);
    } else {
      printf("Replay step %zu/%zu: %s\n",
             step_index + 1u,
             sizeof(k_replay_steps) / sizeof(k_replay_steps[0]),
             step->label);
    }
    if (mini_gnb_c_send_ngap(socket_fd, socket_name, step->label, tx_message, tx_message_length) != 0) {
      close(socket_fd);
      mini_gnb_c_free_pcap_frames(frames, frame_count);
      return 1;
    }

    if (step->wait_for_response) {
      if (mini_gnb_c_recv_ngap(socket_fd, response, sizeof(response), &response_length) != 0) {
        close(socket_fd);
        mini_gnb_c_free_pcap_frames(frames, frame_count);
        return 1;
      }

      if (mini_gnb_c_print_received_ngap(response, response_length) != 0) {
        close(socket_fd);
        mini_gnb_c_free_pcap_frames(frames, frame_count);
        return 1;
      }

      if (step->frame_number == 5u && response_length >= 2u && response[0] == 0x00u && response[1] == 0x09u) {
        fprintf(stderr,
                "Replay hit ErrorIndication immediately after IdentityResponse. "
                "Static NAS replay is insufficient here; the next stage must generate live UE NAS/AKA "
                "(AuthenticationResponse, SecurityModeComplete, RegistrationComplete, and PDU session NAS) "
                "from the AMF's runtime challenge and security context.\n");
      }

      if (mini_gnb_c_expect_ngap(response,
                                 response_length,
                                 step->expected_pdu_type,
                                 step->expected_proc_code,
                                 step->label) != 0) {
        close(socket_fd);
        mini_gnb_c_free_pcap_frames(frames, frame_count);
        return 2;
      }

      if (step->frame_number == 3u &&
          mini_gnb_c_extract_amf_ue_ngap_id(response, response_length, runtime_ue_token) == 0) {
        runtime_ue_token_valid = 1;
        printf("Captured runtime UE NGAP token bytes: %02x %02x\n",
               runtime_ue_token[0],
               runtime_ue_token[1]);
      }
      if (step->frame_number == 5u) {
        uint8_t nas_message_type = mini_gnb_c_extract_nas_message_type(response, response_length);

        if (mini_gnb_c_extract_auth_request(response, response_length, &aka_request) != 0) {
          fprintf(stderr, "failed to parse AuthenticationRequest from AMF response\n");
          close(socket_fd);
          mini_gnb_c_free_pcap_frames(frames, frame_count);
          return 1;
        }

        aka_request_valid = 1;
        printf("Parsed AMF NAS message after IdentityResponse: %s (0x%02x)\n",
               mini_gnb_c_nas_message_name(nas_message_type),
               nas_message_type);
      }
      if (step->frame_number == 7u) {
        uint8_t nas_message_type = mini_gnb_c_extract_nas_message_type(response, response_length);

        printf("Parsed AMF NAS message after AuthenticationResponse: %s (0x%02x)\n",
               mini_gnb_c_nas_message_name(nas_message_type),
               nas_message_type);
        if (nas_message_type == 0x58u) {
          fprintf(stderr, "AMF returned AuthenticationReject after runtime AuthenticationResponse.\n");
          close(socket_fd);
          mini_gnb_c_free_pcap_frames(frames, frame_count);
          return 2;
        }
        if (nas_message_type != 0x5du) {
          fprintf(stderr, "expected SecurityModeCommand after AuthenticationResponse, got NAS message 0x%02x\n",
                  nas_message_type);
          close(socket_fd);
          mini_gnb_c_free_pcap_frames(frames, frame_count);
          return 2;
        }
        if (mini_gnb_c_extract_security_mode_command_algorithms(response,
                                                                response_length,
                                                                &aka_result.ciphering_algorithm,
                                                                &aka_result.integrity_algorithm) != 0) {
          fprintf(stderr, "failed to extract NAS algorithms from SecurityModeCommand\n");
          close(socket_fd);
          mini_gnb_c_free_pcap_frames(frames, frame_count);
          return 2;
        }
        ogs_kdf_nas_5gs(MINI_GNB_C_KDF_NAS_ENC_ALG,
                        aka_result.ciphering_algorithm,
                        aka_result.kamf,
                        aka_result.knas_enc);
        ogs_kdf_nas_5gs(MINI_GNB_C_KDF_NAS_INT_ALG,
                        aka_result.integrity_algorithm,
                        aka_result.kamf,
                        aka_result.knas_int);
        printf("Derived NAS keys from SecurityModeCommand. cipher=0x%02x integrity=0x%02x\n",
               aka_result.ciphering_algorithm,
               aka_result.integrity_algorithm);
      }
      if (step->frame_number == 14u) {
        if (mini_gnb_c_extract_open5gs_upf_tunnel(response, response_length, &core_session) == 0) {
          printf("Parsed UPF tunnel from PDUSessionResourceSetupRequest: %s TEID=0x%08x\n",
                 core_session.upf_ip,
                 core_session.upf_teid);
        } else {
          fprintf(stderr, "warning: failed to parse UPF tunnel from PDUSessionResourceSetupRequest\n");
        }

        if (mini_gnb_c_extract_open5gs_qfi(response, response_length, &core_session) == 0) {
          printf("Parsed QFI from PDUSessionResourceSetupRequest: %u\n", core_session.qfi);
        } else {
          fprintf(stderr, "warning: failed to parse QFI from PDUSessionResourceSetupRequest\n");
        }

        if (mini_gnb_c_extract_open5gs_ue_ipv4(response, response_length, &core_session) == 0) {
          printf("Parsed UE IPv4 from PDU Session Establishment Accept: %u.%u.%u.%u\n",
                 core_session.ue_ipv4[0],
                 core_session.ue_ipv4[1],
                 core_session.ue_ipv4[2],
                 core_session.ue_ipv4[3]);
        } else {
          fprintf(stderr, "warning: failed to parse UE IPv4 from PDU Session Establishment Accept\n");
        }
      }
    } else if (step->post_send_delay_ms > 0u) {
      (void)poll(NULL, 0, (int)step->post_send_delay_ms);
    }
  }

  printf("Replay reached PDU Session Resource Setup Response.\n");
  printf("This confirms N2 attach/session signaling up to SMF/UPF resource setup against the AMF.\n");

  if (mini_gnb_c_run_gtpu_echo_probe(options, &core_session) != 0) {
    close(socket_fd);
    mini_gnb_c_free_pcap_frames(frames, frame_count);
    return 1;
  }

  if (mini_gnb_c_send_gtpu_gpdu(options, &core_session) != 0) {
    close(socket_fd);
    mini_gnb_c_free_pcap_frames(frames, frame_count);
    return 1;
  }

  close(socket_fd);
  mini_gnb_c_free_pcap_frames(frames, frame_count);
  return 0;
}

static int mini_gnb_c_parse_options(int argc, char** argv, mini_gnb_c_probe_options_t* options) {
  int arg_index = 0;
  int positional_count = 0;
  const char* positional_args[3];

  if (options == NULL) {
    return -1;
  }

  options->mode = MINI_GNB_C_MODE_SETUP;
  options->amf_ip = MINI_GNB_C_DEFAULT_AMF_IP;
  options->amf_port = MINI_GNB_C_DEFAULT_AMF_PORT;
  options->timeout_ms = MINI_GNB_C_DEFAULT_TIMEOUT_MS;
  options->replay_pcap_path = MINI_GNB_C_DEFAULT_REPLAY_PCAP;
  options->imsi = MINI_GNB_C_DEFAULT_IMSI;
  options->key_hex = MINI_GNB_C_DEFAULT_KEY;
  options->opc_hex = MINI_GNB_C_DEFAULT_OPC;
  options->auth_amf_hex = MINI_GNB_C_DEFAULT_AUTH_AMF;
  options->mcc = MINI_GNB_C_DEFAULT_MCC;
  options->mnc = MINI_GNB_C_DEFAULT_MNC;
  options->upf_ip = MINI_GNB_C_DEFAULT_UPF_IP;
  options->upf_port = MINI_GNB_C_DEFAULT_UPF_PORT;
  options->gpdu_dst_ip = MINI_GNB_C_DEFAULT_GPDU_DST_IP;
  options->ngap_trace_pcap_path = MINI_GNB_C_DEFAULT_NGAP_TRACE_PCAP;
  options->gtpu_trace_pcap_path = MINI_GNB_C_DEFAULT_GTPU_TRACE_PCAP;

  for (arg_index = 1; arg_index < argc; ++arg_index) {
    if (strcmp(argv[arg_index], "--help") == 0) {
      return 1;
    }
    if (strcmp(argv[arg_index], "--replay") == 0) {
      options->mode = MINI_GNB_C_MODE_REPLAY;
      continue;
    }
    if (strcmp(argv[arg_index], "--pcap") == 0) {
      if (arg_index + 1 >= argc) {
        fprintf(stderr, "--pcap requires a path\n");
        return -1;
      }
      options->replay_pcap_path = argv[++arg_index];
      continue;
    }
    if (strcmp(argv[arg_index], "--imsi") == 0) {
      if (arg_index + 1 >= argc) {
        fprintf(stderr, "--imsi requires a value\n");
        return -1;
      }
      options->imsi = argv[++arg_index];
      continue;
    }
    if (strcmp(argv[arg_index], "--key") == 0) {
      if (arg_index + 1 >= argc) {
        fprintf(stderr, "--key requires a value\n");
        return -1;
      }
      options->key_hex = argv[++arg_index];
      continue;
    }
    if (strcmp(argv[arg_index], "--opc") == 0) {
      if (arg_index + 1 >= argc) {
        fprintf(stderr, "--opc requires a value\n");
        return -1;
      }
      options->opc_hex = argv[++arg_index];
      continue;
    }
    if (strcmp(argv[arg_index], "--auth-amf") == 0) {
      if (arg_index + 1 >= argc) {
        fprintf(stderr, "--auth-amf requires a value\n");
        return -1;
      }
      options->auth_amf_hex = argv[++arg_index];
      continue;
    }
    if (strcmp(argv[arg_index], "--mcc") == 0) {
      if (arg_index + 1 >= argc) {
        fprintf(stderr, "--mcc requires a value\n");
        return -1;
      }
      options->mcc = argv[++arg_index];
      continue;
    }
    if (strcmp(argv[arg_index], "--mnc") == 0) {
      if (arg_index + 1 >= argc) {
        fprintf(stderr, "--mnc requires a value\n");
        return -1;
      }
      options->mnc = argv[++arg_index];
      continue;
    }
    if (strcmp(argv[arg_index], "--upf-ip") == 0) {
      if (arg_index + 1 >= argc) {
        fprintf(stderr, "--upf-ip requires a value\n");
        return -1;
      }
      options->upf_ip = argv[++arg_index];
      continue;
    }
    if (strcmp(argv[arg_index], "--upf-port") == 0) {
      if (arg_index + 1 >= argc) {
        fprintf(stderr, "--upf-port requires a value\n");
        return -1;
      }
      if (mini_gnb_c_parse_u32(argv[++arg_index], &options->upf_port) != 0) {
        fprintf(stderr, "invalid upf-port: %s\n", argv[arg_index]);
        return -1;
      }
      continue;
    }
    if (strcmp(argv[arg_index], "--gpdu-dst-ip") == 0) {
      if (arg_index + 1 >= argc) {
        fprintf(stderr, "--gpdu-dst-ip requires a value\n");
        return -1;
      }
      options->gpdu_dst_ip = argv[++arg_index];
      continue;
    }
    if (strcmp(argv[arg_index], "--ngap-trace-pcap") == 0) {
      if (arg_index + 1 >= argc) {
        fprintf(stderr, "--ngap-trace-pcap requires a value\n");
        return -1;
      }
      options->ngap_trace_pcap_path = argv[++arg_index];
      continue;
    }
    if (strcmp(argv[arg_index], "--gtpu-trace-pcap") == 0) {
      if (arg_index + 1 >= argc) {
        fprintf(stderr, "--gtpu-trace-pcap requires a value\n");
        return -1;
      }
      options->gtpu_trace_pcap_path = argv[++arg_index];
      continue;
    }

    if (positional_count >= 3) {
      fprintf(stderr, "too many positional arguments\n");
      return -1;
    }
    positional_args[positional_count++] = argv[arg_index];
  }

  if (positional_count >= 1 && positional_args[0] != NULL && positional_args[0][0] != '\0') {
    options->amf_ip = positional_args[0];
  }
  if (positional_count >= 2 && mini_gnb_c_parse_u32(positional_args[1], &options->amf_port) != 0) {
    fprintf(stderr, "invalid amf_port: %s\n", positional_args[1]);
    return -1;
  }
  if (positional_count >= 3 && mini_gnb_c_parse_u32(positional_args[2], &options->timeout_ms) != 0) {
    fprintf(stderr, "invalid timeout_ms: %s\n", positional_args[2]);
    return -1;
  }

  return 0;
}

int main(int argc, char** argv) {
  mini_gnb_c_probe_options_t options;
  int exit_code = 0;

  signal(SIGPIPE, SIG_IGN);

  switch (mini_gnb_c_parse_options(argc, argv, &options)) {
    case 0:
      break;
    case 1:
      mini_gnb_c_print_usage(argv[0]);
      return 0;
    default:
      mini_gnb_c_print_usage(argv[0]);
      return 1;
  }

  mini_gnb_c_trace_init(&options);

  if (options.mode == MINI_GNB_C_MODE_REPLAY) {
    if (mini_gnb_c_replay_socket_type("SOCK_STREAM", SOCK_STREAM, &options) == 0) {
      exit_code = 0;
      goto done;
    }

    fprintf(stderr, "SOCK_STREAM replay failed, trying SOCK_SEQPACKET for diagnostics...\n");
    exit_code = mini_gnb_c_replay_socket_type("SOCK_SEQPACKET", SOCK_SEQPACKET, &options);
    goto done;
  }

  if (mini_gnb_c_probe_socket_type("SOCK_STREAM", SOCK_STREAM, &options) == 0) {
    exit_code = 0;
    goto done;
  }

  fprintf(stderr, "SOCK_STREAM probe failed, trying SOCK_SEQPACKET for diagnostics...\n");
  exit_code = mini_gnb_c_probe_socket_type("SOCK_SEQPACKET", SOCK_SEQPACKET, &options);

done:
  mini_gnb_c_trace_close();
  return exit_code;
}
