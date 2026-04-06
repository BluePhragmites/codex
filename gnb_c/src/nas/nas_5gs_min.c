#include "mini_gnb_c/nas/nas_5gs_min.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mini_gnb_c/common/hex.h"
#include "mini_gnb_c/common/json_utils.h"
#include "mini_gnb_c/link/json_link.h"

#define MINI_GNB_C_NAS_5GS_MIN_KAUSF_LEN 32u
#define MINI_GNB_C_NAS_5GS_MIN_KSEAF_LEN 32u
#define MINI_GNB_C_NAS_5GS_MIN_KAMF_LEN 32u
#define MINI_GNB_C_NAS_5GS_MIN_KNAS_LEN 16u
#define MINI_GNB_C_NAS_5GS_MIN_BEARER_3GPP 1u
#define MINI_GNB_C_NAS_5GS_MIN_DIRECTION_UPLINK 0u
#define MINI_GNB_C_NAS_5GS_MIN_KDF_NAS_ENC_ALG 0x01u
#define MINI_GNB_C_NAS_5GS_MIN_KDF_NAS_INT_ALG 0x02u
#define MINI_GNB_C_NAS_5GS_MIN_NAS_ALG_NEA0 0u
#define MINI_GNB_C_NAS_5GS_MIN_NAS_ALG_NIA2 2u
/*
 * In the shared-slot live path the UE consumes one gNB slot summary after the
 * simulator has already polled UL_NAS for that same absolute slot. Schedule
 * follow-up NAS one extra slot later so the bridge sees it as due instead of
 * stale on the next poll.
 */
#define MINI_GNB_C_NAS_5GS_MIN_RESPONSE_DELAY_SLOTS 4

int milenage_f2345(const uint8_t* opc,
                   const uint8_t* k,
                   const uint8_t* rand,
                   uint8_t* res,
                   uint8_t* ck,
                   uint8_t* ik,
                   uint8_t* ak,
                   uint8_t* akstar);
void ogs_kdf_kausf(uint8_t* ck, uint8_t* ik, char* serving_network_name, uint8_t* autn, uint8_t* kausf);
void ogs_kdf_kseaf(char* serving_network_name, const uint8_t* kausf, uint8_t* kseaf);
void ogs_kdf_kamf(const char* supi, const uint8_t* abba, uint8_t abba_len, const uint8_t* kseaf, uint8_t* kamf);
void ogs_kdf_nas_5gs(uint8_t algorithm_type_distinguishers,
                     uint8_t algorithm_identity,
                     const uint8_t* kamf,
                     uint8_t* knas);
void ogs_kdf_xres_star(uint8_t* ck,
                       uint8_t* ik,
                       char* serving_network_name,
                       uint8_t* rand,
                       uint8_t* xres,
                       size_t xres_len,
                       uint8_t* xres_star);
int ogs_aes_cmac_calculate(uint8_t* cmac, const uint8_t* key, const uint8_t* msg, uint32_t len);

static const char* k_mini_gnb_c_default_imsi = "460991234567898";
static const char* k_mini_gnb_c_default_key = "11111111 11111111 11111111 11111111";
static const char* k_mini_gnb_c_default_opc = "11111111 11111111 11111111 11111111";
static const char* k_mini_gnb_c_default_auth_amf = "8000";
static const char* k_mini_gnb_c_default_mcc = "460";
static const char* k_mini_gnb_c_default_mnc = "99";

static const uint8_t k_mini_gnb_c_identity_response_nas[] = {
    0x7e, 0x00, 0x5c, 0x00, 0x0d, 0x01, 0x64, 0xf0, 0x99, 0xf0, 0xff, 0x00, 0x00, 0x21, 0x43, 0x65, 0x87, 0x89,
};
static const uint8_t k_mini_gnb_c_security_mode_complete_nas_template[] = {
    0x7e, 0x04, 0x1f, 0x7c, 0xe9, 0x75, 0x00, 0x7e, 0x00, 0x5e, 0x77, 0x00, 0x09, 0x85,
    0x26, 0x61, 0x09, 0x56, 0x16, 0x39, 0x78, 0xf8, 0x71, 0x00, 0x2e, 0x7e, 0x00, 0x41,
    0x39, 0x00, 0x0b, 0xf2, 0x64, 0xf0, 0x99, 0x02, 0x00, 0x40, 0xc0, 0x00, 0x06, 0x01,
    0x10, 0x01, 0x07, 0x2e, 0x02, 0xf0, 0xf0, 0x52, 0x64, 0xf0, 0x99, 0x00, 0x00, 0x01,
    0x17, 0x07, 0xf0, 0xf0, 0xc0, 0xc0, 0x1d, 0x80, 0x30, 0x18, 0x01, 0x00, 0x53, 0x01,
    0x01,
};
static const uint8_t k_mini_gnb_c_registration_complete_nas_template[] = {
    0x7e, 0x02, 0x0b, 0x94, 0x35, 0xf4, 0x01, 0x7e, 0x00, 0x43,
};
static const uint8_t k_mini_gnb_c_pdu_session_establishment_request_nas_template[] = {
    0x7e, 0x02, 0x7e, 0xa9, 0x5c, 0x08, 0x02, 0x7e, 0x00, 0x67, 0x01, 0x00, 0x33, 0x2e,
    0x01, 0x01, 0xc1, 0xff, 0xff, 0x91, 0x28, 0x01, 0x00, 0x55, 0x10, 0x00, 0x7b, 0x00,
    0x23, 0x80, 0x80, 0x21, 0x10, 0x01, 0x01, 0x00, 0x10, 0x81, 0x06, 0x00, 0x00, 0x00,
    0x00, 0x83, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x0d, 0x00, 0x00,
    0x05, 0x00, 0x00, 0x11, 0x00, 0x00, 0x10, 0x00, 0x12, 0x01, 0x81, 0x25, 0x08, 0x07,
    0x64, 0x65, 0x66, 0x61, 0x75, 0x6c, 0x74,
};

static const char* mini_gnb_c_nas_5gs_min_uplink_kind_name(const mini_gnb_c_nas_5gs_min_uplink_kind_t kind) {
  switch (kind) {
    case MINI_GNB_C_NAS_5GS_MIN_UPLINK_IDENTITY_RESPONSE:
      return "IdentityResponse";
    case MINI_GNB_C_NAS_5GS_MIN_UPLINK_AUTHENTICATION_RESPONSE:
      return "AuthenticationResponse";
    case MINI_GNB_C_NAS_5GS_MIN_UPLINK_SECURITY_MODE_COMPLETE:
      return "SecurityModeComplete";
    case MINI_GNB_C_NAS_5GS_MIN_UPLINK_REGISTRATION_COMPLETE:
      return "RegistrationComplete";
    case MINI_GNB_C_NAS_5GS_MIN_UPLINK_PDU_SESSION_REQUEST:
      return "PDUSessionEstablishmentRequest";
    default:
      return "unknown";
  }
}

static int mini_gnb_c_nas_5gs_min_parse_hex_string(const char* text, uint8_t* output, size_t output_len) {
  size_t out_index = 0u;
  int high_nibble = -1;

  if (text == NULL || output == NULL) {
    return -1;
  }

  for (; *text != '\0'; ++text) {
    int nibble = -1;
    const char ch = *text;

    if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
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
      continue;
    }
    if (out_index >= output_len) {
      return -1;
    }
    output[out_index++] = (uint8_t)((high_nibble << 4) | nibble);
    high_nibble = -1;
  }

  return high_nibble < 0 && out_index == output_len ? 0 : -1;
}

static void mini_gnb_c_nas_5gs_min_format_serving_network_name(char* buffer, size_t buffer_len) {
  char mnc_padded[4];

  if (buffer == NULL || buffer_len == 0u) {
    return;
  }
  if (strlen(k_mini_gnb_c_default_mnc) == 2u) {
    (void)snprintf(mnc_padded, sizeof(mnc_padded), "0%s", k_mini_gnb_c_default_mnc);
  } else {
    (void)snprintf(mnc_padded, sizeof(mnc_padded), "%s", k_mini_gnb_c_default_mnc);
  }
  (void)snprintf(buffer,
                 buffer_len,
                 "5G:mnc%s.mcc%s.3gppnetwork.org",
                 mnc_padded,
                 k_mini_gnb_c_default_mcc);
}

static const uint8_t* mini_gnb_c_nas_5gs_min_find_message(const uint8_t* bytes,
                                                          size_t length,
                                                          uint8_t message_type,
                                                          size_t* offset_out) {
  size_t index = 0u;

  if (bytes == NULL || offset_out == NULL || length < 3u) {
    return NULL;
  }
  for (index = 0u; index + 2u < length; ++index) {
    if (bytes[index] == 0x7eu && bytes[index + 1u] == 0x00u && bytes[index + 2u] == message_type) {
      *offset_out = index;
      return bytes + index;
    }
  }
  return NULL;
}

uint8_t mini_gnb_c_nas_5gs_min_extract_message_type(const uint8_t* nas_pdu, size_t nas_pdu_length) {
  size_t offset = 0u;
  static const uint8_t k_types[] = {0x5bu, 0x5cu, 0x54u, 0x56u, 0x57u, 0x58u, 0x5du, 0x5eu, 0x43u, 0x67u};
  size_t index = 0u;

  for (index = 0u; index < sizeof(k_types); ++index) {
    if (mini_gnb_c_nas_5gs_min_find_message(nas_pdu, nas_pdu_length, k_types[index], &offset) != NULL) {
      return k_types[index];
    }
  }
  return 0x00u;
}

static int mini_gnb_c_nas_5gs_min_extract_auth_request(const uint8_t* nas_pdu,
                                                       size_t nas_pdu_length,
                                                       mini_gnb_c_nas_5gs_min_security_context_t* context) {
  const uint8_t* auth_request = NULL;
  size_t offset = 0u;

  if (nas_pdu == NULL || context == NULL) {
    return -1;
  }
  memset(context, 0, sizeof(*context));
  auth_request = mini_gnb_c_nas_5gs_min_find_message(nas_pdu, nas_pdu_length, 0x56u, &offset);
  if (auth_request == NULL || offset + 5u > nas_pdu_length) {
    return -1;
  }
  context->abba_len = (size_t)auth_request[4];
  if (context->abba_len > sizeof(context->abba) ||
      offset + 5u + context->abba_len + 1u + 16u + 2u + 16u > nas_pdu_length) {
    return -1;
  }
  memcpy(context->abba, auth_request + 5u, context->abba_len);
  if (auth_request[5u + context->abba_len] != 0x21u) {
    return -1;
  }
  memcpy(context->rand, auth_request + 6u + context->abba_len, sizeof(context->rand));
  if (auth_request[22u + context->abba_len] != 0x20u || auth_request[23u + context->abba_len] != 0x10u) {
    return -1;
  }
  memcpy(context->autn, auth_request + 24u + context->abba_len, sizeof(context->autn));
  return 0;
}

static int mini_gnb_c_nas_5gs_min_extract_security_mode_algorithms(const uint8_t* nas_pdu,
                                                                   size_t nas_pdu_length,
                                                                   uint8_t* ciphering_algorithm,
                                                                   uint8_t* integrity_algorithm) {
  const uint8_t* command = NULL;
  size_t offset = 0u;

  if (ciphering_algorithm == NULL || integrity_algorithm == NULL) {
    return -1;
  }
  command = mini_gnb_c_nas_5gs_min_find_message(nas_pdu, nas_pdu_length, 0x5du, &offset);
  if (command == NULL || offset + 4u > nas_pdu_length) {
    return -1;
  }

  *ciphering_algorithm = (uint8_t)((command[3] >> 4u) & 0x0fu);
  *integrity_algorithm = (uint8_t)(command[3] & 0x0fu);
  return 0;
}

static int mini_gnb_c_nas_5gs_min_calculate_mac(const uint8_t* knas_int,
                                                uint32_t count,
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
  cmac_input[4] = (uint8_t)((MINI_GNB_C_NAS_5GS_MIN_BEARER_3GPP << 3u) |
                            (MINI_GNB_C_NAS_5GS_MIN_DIRECTION_UPLINK << 2u));
  memcpy(cmac_input + 8u, message, message_len);
  if (ogs_aes_cmac_calculate(cmac, knas_int, cmac_input, (uint32_t)(message_len + 8u)) != 0) {
    free(cmac_input);
    return -1;
  }

  memcpy(mac, cmac, 4u);
  free(cmac_input);
  return 0;
}

static int mini_gnb_c_nas_5gs_min_patch_protected_uplink_message(uint8_t* nas_pdu,
                                                                 size_t nas_pdu_length,
                                                                 const mini_gnb_c_nas_5gs_min_security_context_t* context) {
  uint8_t mac[4];
  uint32_t ul_count = 0u;

  if (nas_pdu == NULL || context == NULL || nas_pdu_length < 8u) {
    return -1;
  }

  ul_count = (uint32_t)nas_pdu[6];
  if (mini_gnb_c_nas_5gs_min_calculate_mac(context->knas_int, ul_count, nas_pdu + 6u, nas_pdu_length - 6u, mac) !=
      0) {
    return -1;
  }
  memcpy(nas_pdu + 2u, mac, sizeof(mac));
  return 0;
}

static int mini_gnb_c_nas_5gs_min_build_authentication_response(
    mini_gnb_c_nas_5gs_min_security_context_t* context,
    uint8_t* nas_out,
    size_t nas_capacity,
    size_t* nas_length_out) {
  uint8_t key[16];
  uint8_t opc[16];
  uint8_t auth_amf[2];
  char serving_network_name[64];

  if (context == NULL || nas_out == NULL || nas_length_out == NULL || nas_capacity < 21u) {
    return -1;
  }
  if (mini_gnb_c_nas_5gs_min_parse_hex_string(k_mini_gnb_c_default_key, key, sizeof(key)) != 0 ||
      mini_gnb_c_nas_5gs_min_parse_hex_string(k_mini_gnb_c_default_opc, opc, sizeof(opc)) != 0 ||
      mini_gnb_c_nas_5gs_min_parse_hex_string(k_mini_gnb_c_default_auth_amf, auth_amf, sizeof(auth_amf)) != 0) {
    return -1;
  }

  context->res_len = 8u;
  context->res_star_len = 16u;
  context->ciphering_algorithm = MINI_GNB_C_NAS_5GS_MIN_NAS_ALG_NEA0;
  context->integrity_algorithm = MINI_GNB_C_NAS_5GS_MIN_NAS_ALG_NIA2;
  mini_gnb_c_nas_5gs_min_format_serving_network_name(serving_network_name, sizeof(serving_network_name));

  if (milenage_f2345(opc,
                     key,
                     context->rand,
                     context->res,
                     context->ck,
                     context->ik,
                     context->ak,
                     NULL) != 0) {
    return -1;
  }

  for (size_t index = 0u; index < sizeof(context->sqn); ++index) {
    context->sqn[index] = context->autn[index] ^ context->ak[index];
  }
  ogs_kdf_xres_star(context->ck,
                    context->ik,
                    serving_network_name,
                    context->rand,
                    context->res,
                    context->res_len,
                    context->res_star);
  {
    char supi[32];

    (void)snprintf(supi, sizeof(supi), "imsi-%s", k_mini_gnb_c_default_imsi);
    ogs_kdf_kausf(context->ck, context->ik, serving_network_name, context->autn, context->kausf);
    ogs_kdf_kseaf(serving_network_name, context->kausf, context->kseaf);
    ogs_kdf_kamf(supi, context->abba, (uint8_t)context->abba_len, context->kseaf, context->kamf);
  }
  ogs_kdf_nas_5gs(MINI_GNB_C_NAS_5GS_MIN_KDF_NAS_ENC_ALG,
                  context->ciphering_algorithm,
                  context->kamf,
                  context->knas_enc);
  ogs_kdf_nas_5gs(MINI_GNB_C_NAS_5GS_MIN_KDF_NAS_INT_ALG,
                  context->integrity_algorithm,
                  context->kamf,
                  context->knas_int);

  nas_out[0] = 0x7eu;
  nas_out[1] = 0x00u;
  nas_out[2] = 0x57u;
  nas_out[3] = 0x2du;
  nas_out[4] = 0x10u;
  memcpy(nas_out + 5u, context->res_star, 16u);
  *nas_length_out = 21u;
  (void)auth_amf;
  return 0;
}

static int mini_gnb_c_nas_5gs_min_queue_message(mini_gnb_c_nas_5gs_min_ue_t* runtime,
                                                mini_gnb_c_nas_5gs_min_uplink_kind_t kind,
                                                const uint8_t* nas_pdu,
                                                size_t nas_pdu_length,
                                                int abs_slot) {
  size_t index = 0u;
  mini_gnb_c_nas_5gs_min_pending_uplink_t pending;

  if (runtime == NULL || nas_pdu == NULL || nas_pdu_length == 0u || nas_pdu_length > sizeof(pending.nas_pdu.bytes) ||
      abs_slot < 0) {
    return -1;
  }
  for (index = 0u; index < MINI_GNB_C_NAS_5GS_MIN_MAX_PENDING; ++index) {
    if (!runtime->pending[index].valid) {
      memset(&pending, 0, sizeof(pending));
      pending.valid = true;
      pending.abs_slot = abs_slot;
      pending.kind = kind;
      if (mini_gnb_c_buffer_set_bytes(&pending.nas_pdu, nas_pdu, nas_pdu_length) != 0) {
        return -1;
      }
      runtime->pending[index] = pending;
      return 0;
    }
  }
  return -1;
}

static mini_gnb_c_nas_5gs_min_pending_uplink_t* mini_gnb_c_nas_5gs_min_find_next_due(
    mini_gnb_c_nas_5gs_min_ue_t* runtime,
    int current_slot) {
  mini_gnb_c_nas_5gs_min_pending_uplink_t* match = NULL;
  size_t index = 0u;

  if (runtime == NULL) {
    return NULL;
  }
  for (index = 0u; index < MINI_GNB_C_NAS_5GS_MIN_MAX_PENDING; ++index) {
    mini_gnb_c_nas_5gs_min_pending_uplink_t* candidate = &runtime->pending[index];

    if (!candidate->valid || candidate->abs_slot > current_slot) {
      continue;
    }
    if (match == NULL || candidate->abs_slot < match->abs_slot) {
      match = candidate;
    }
  }
  return match;
}

static void mini_gnb_c_nas_5gs_min_mark_sent(mini_gnb_c_nas_5gs_min_ue_t* runtime,
                                             mini_gnb_c_nas_5gs_min_uplink_kind_t kind) {
  switch (kind) {
    case MINI_GNB_C_NAS_5GS_MIN_UPLINK_IDENTITY_RESPONSE:
      runtime->identity_response_pending = false;
      runtime->identity_response_sent = true;
      break;
    case MINI_GNB_C_NAS_5GS_MIN_UPLINK_AUTHENTICATION_RESPONSE:
      runtime->authentication_response_pending = false;
      runtime->authentication_response_sent = true;
      break;
    case MINI_GNB_C_NAS_5GS_MIN_UPLINK_SECURITY_MODE_COMPLETE:
      runtime->security_mode_complete_pending = false;
      runtime->security_mode_complete_sent = true;
      break;
    case MINI_GNB_C_NAS_5GS_MIN_UPLINK_REGISTRATION_COMPLETE:
      runtime->registration_complete_pending = false;
      runtime->registration_complete_sent = true;
      break;
    case MINI_GNB_C_NAS_5GS_MIN_UPLINK_PDU_SESSION_REQUEST:
      runtime->pdu_session_request_pending = false;
      runtime->pdu_session_request_sent = true;
      break;
    default:
      break;
  }
}

static int mini_gnb_c_nas_5gs_min_emit_one_uplink(mini_gnb_c_nas_5gs_min_ue_t* runtime,
                                                  const char* exchange_dir,
                                                  mini_gnb_c_nas_5gs_min_pending_uplink_t* pending,
                                                  int current_slot) {
  char nas_hex[MINI_GNB_C_MAX_PAYLOAD * 2u + 1u];
  char payload_json[MINI_GNB_C_MAX_PAYLOAD * 2u + 256u];
  int visible_abs_slot = 0;

  if (runtime == NULL || exchange_dir == NULL || pending == NULL || !pending->valid || !runtime->c_rnti_valid) {
    return -1;
  }
  if (mini_gnb_c_bytes_to_hex(pending->nas_pdu.bytes, pending->nas_pdu.len, nas_hex, sizeof(nas_hex)) != 0) {
    return -1;
  }
  if (snprintf(payload_json,
               sizeof(payload_json),
               "{\"c_rnti\":%u,\"ran_ue_ngap_id\":%u,\"amf_ue_ngap_id\":%u,\"nas_hex\":\"%s\"}",
               (unsigned)runtime->c_rnti,
               (unsigned)(runtime->ran_ue_ngap_id_valid ? runtime->ran_ue_ngap_id : 0u),
               (unsigned)(runtime->amf_ue_ngap_id_valid ? runtime->amf_ue_ngap_id : 0u),
               nas_hex) >= (int)sizeof(payload_json)) {
    return -1;
  }
  visible_abs_slot = current_slot + 1;
  if (mini_gnb_c_json_link_emit_event(exchange_dir,
                                      "ue_to_gnb_nas",
                                      "ue",
                                      "UL_NAS",
                                      runtime->next_ue_to_gnb_nas_sequence++,
                                      visible_abs_slot,
                                      payload_json,
                                      NULL,
                                      0u) != 0) {
    return -1;
  }
  printf("UE emitted auto UL_NAS type=%s visible_abs_slot=%d seq=%u\n",
         mini_gnb_c_nas_5gs_min_uplink_kind_name(pending->kind),
         visible_abs_slot,
         runtime->next_ue_to_gnb_nas_sequence - 1u);
  fflush(stdout);
  mini_gnb_c_nas_5gs_min_mark_sent(runtime, pending->kind);
  memset(pending, 0, sizeof(*pending));
  return 0;
}

void mini_gnb_c_nas_5gs_min_ue_init(mini_gnb_c_nas_5gs_min_ue_t* runtime) {
  if (runtime == NULL) {
    return;
  }
  memset(runtime, 0, sizeof(*runtime));
  runtime->next_gnb_to_ue_sequence = 1u;
  runtime->next_ue_to_gnb_nas_sequence = 1u;
}

int mini_gnb_c_nas_5gs_min_handle_downlink_nas(mini_gnb_c_nas_5gs_min_ue_t* runtime,
                                               uint16_t c_rnti,
                                               uint16_t ran_ue_ngap_id,
                                               bool ran_ue_ngap_id_valid,
                                               uint16_t amf_ue_ngap_id,
                                               bool amf_ue_ngap_id_valid,
                                               const uint8_t* nas_pdu,
                                               size_t nas_pdu_length,
                                               int current_slot) {
  uint8_t message[MINI_GNB_C_MAX_PAYLOAD];
  size_t message_length = 0u;
  uint8_t message_type = 0u;

  if (runtime == NULL || nas_pdu == NULL || nas_pdu_length == 0u) {
    return -1;
  }
  runtime->c_rnti = c_rnti;
  runtime->c_rnti_valid = c_rnti != 0u;
  runtime->ran_ue_ngap_id = ran_ue_ngap_id;
  runtime->ran_ue_ngap_id_valid = ran_ue_ngap_id_valid;
  runtime->amf_ue_ngap_id = amf_ue_ngap_id;
  runtime->amf_ue_ngap_id_valid = amf_ue_ngap_id_valid;
  message_type = mini_gnb_c_nas_5gs_min_extract_message_type(nas_pdu, nas_pdu_length);

  if (message_type == 0x5bu && !runtime->identity_response_pending && !runtime->identity_response_sent) {
    if (mini_gnb_c_nas_5gs_min_queue_message(runtime,
                                             MINI_GNB_C_NAS_5GS_MIN_UPLINK_IDENTITY_RESPONSE,
                                             k_mini_gnb_c_identity_response_nas,
                                             sizeof(k_mini_gnb_c_identity_response_nas),
                                             current_slot + MINI_GNB_C_NAS_5GS_MIN_RESPONSE_DELAY_SLOTS) != 0) {
      return -1;
    }
    runtime->identity_response_pending = true;
    return 0;
  }

  if (message_type == 0x56u && !runtime->authentication_response_pending && !runtime->authentication_response_sent) {
    if (mini_gnb_c_nas_5gs_min_extract_auth_request(nas_pdu, nas_pdu_length, &runtime->security_context) != 0) {
      return -1;
    }
    if (mini_gnb_c_nas_5gs_min_build_authentication_response(&runtime->security_context,
                                                             message,
                                                             sizeof(message),
                                                             &message_length) != 0 ||
        mini_gnb_c_nas_5gs_min_queue_message(runtime,
                                             MINI_GNB_C_NAS_5GS_MIN_UPLINK_AUTHENTICATION_RESPONSE,
                                             message,
                                             message_length,
                                             current_slot + MINI_GNB_C_NAS_5GS_MIN_RESPONSE_DELAY_SLOTS) != 0) {
      return -1;
    }
    runtime->security_context_valid = true;
    runtime->authentication_response_pending = true;
    return 0;
  }

  if (message_type == 0x5du && runtime->security_context_valid && !runtime->security_mode_complete_sent &&
      !runtime->security_mode_complete_pending) {
    if (mini_gnb_c_nas_5gs_min_extract_security_mode_algorithms(nas_pdu,
                                                                nas_pdu_length,
                                                                &runtime->security_context.ciphering_algorithm,
                                                                &runtime->security_context.integrity_algorithm) != 0) {
      return -1;
    }
    ogs_kdf_nas_5gs(MINI_GNB_C_NAS_5GS_MIN_KDF_NAS_ENC_ALG,
                    runtime->security_context.ciphering_algorithm,
                    runtime->security_context.kamf,
                    runtime->security_context.knas_enc);
    ogs_kdf_nas_5gs(MINI_GNB_C_NAS_5GS_MIN_KDF_NAS_INT_ALG,
                    runtime->security_context.integrity_algorithm,
                    runtime->security_context.kamf,
                    runtime->security_context.knas_int);

    memcpy(message,
           k_mini_gnb_c_security_mode_complete_nas_template,
           sizeof(k_mini_gnb_c_security_mode_complete_nas_template));
    message_length = sizeof(k_mini_gnb_c_security_mode_complete_nas_template);
    if (mini_gnb_c_nas_5gs_min_patch_protected_uplink_message(message,
                                                              message_length,
                                                              &runtime->security_context) != 0 ||
        mini_gnb_c_nas_5gs_min_queue_message(runtime,
                                             MINI_GNB_C_NAS_5GS_MIN_UPLINK_SECURITY_MODE_COMPLETE,
                                             message,
                                             message_length,
                                             current_slot + MINI_GNB_C_NAS_5GS_MIN_RESPONSE_DELAY_SLOTS) != 0) {
      return -1;
    }
    runtime->security_mode_complete_pending = true;

    memcpy(message,
           k_mini_gnb_c_registration_complete_nas_template,
           sizeof(k_mini_gnb_c_registration_complete_nas_template));
    message_length = sizeof(k_mini_gnb_c_registration_complete_nas_template);
    if (mini_gnb_c_nas_5gs_min_patch_protected_uplink_message(message,
                                                              message_length,
                                                              &runtime->security_context) != 0 ||
        mini_gnb_c_nas_5gs_min_queue_message(runtime,
                                             MINI_GNB_C_NAS_5GS_MIN_UPLINK_REGISTRATION_COMPLETE,
                                             message,
                                             message_length,
                                             current_slot + MINI_GNB_C_NAS_5GS_MIN_RESPONSE_DELAY_SLOTS + 1) != 0) {
      return -1;
    }
    runtime->registration_complete_pending = true;

    memcpy(message,
           k_mini_gnb_c_pdu_session_establishment_request_nas_template,
           sizeof(k_mini_gnb_c_pdu_session_establishment_request_nas_template));
    message_length = sizeof(k_mini_gnb_c_pdu_session_establishment_request_nas_template);
    if (mini_gnb_c_nas_5gs_min_patch_protected_uplink_message(message,
                                                              message_length,
                                                              &runtime->security_context) != 0 ||
        mini_gnb_c_nas_5gs_min_queue_message(runtime,
                                             MINI_GNB_C_NAS_5GS_MIN_UPLINK_PDU_SESSION_REQUEST,
                                             message,
                                             message_length,
                                             current_slot + MINI_GNB_C_NAS_5GS_MIN_RESPONSE_DELAY_SLOTS + 2) != 0) {
      return -1;
    }
    runtime->pdu_session_request_pending = true;
    return 0;
  }

  return 0;
}

int mini_gnb_c_nas_5gs_min_emit_due_uplinks(mini_gnb_c_nas_5gs_min_ue_t* runtime,
                                            const char* exchange_dir,
                                            int current_slot) {
  mini_gnb_c_nas_5gs_min_pending_uplink_t* pending = NULL;

  if (runtime == NULL || exchange_dir == NULL || exchange_dir[0] == '\0') {
    return 0;
  }

  pending = mini_gnb_c_nas_5gs_min_find_next_due(runtime, current_slot);
  while (pending != NULL) {
    if (mini_gnb_c_nas_5gs_min_emit_one_uplink(runtime, exchange_dir, pending, current_slot) != 0) {
      return -1;
    }
    pending = mini_gnb_c_nas_5gs_min_find_next_due(runtime, current_slot);
  }
  return 0;
}

int mini_gnb_c_nas_5gs_min_poll_exchange(mini_gnb_c_nas_5gs_min_ue_t* runtime,
                                         const char* exchange_dir,
                                         int current_slot) {
  char event_path[MINI_GNB_C_MAX_PATH];

  if (runtime == NULL || exchange_dir == NULL || exchange_dir[0] == '\0') {
    return 0;
  }

  while (mini_gnb_c_json_link_find_event_path(exchange_dir,
                                              "gnb_to_ue",
                                              "gnb",
                                              runtime->next_gnb_to_ue_sequence,
                                              event_path,
                                              sizeof(event_path)) == 0) {
    char* event_text = mini_gnb_c_read_text_file(event_path);
    int abs_slot = 0;
    int c_rnti = 0;
    int ran_ue_ngap_id = 0;
    int amf_ue_ngap_id = 0;
    char nas_hex[MINI_GNB_C_MAX_PAYLOAD * 2u + 1u];
    uint8_t nas_pdu[MINI_GNB_C_MAX_PAYLOAD];
    size_t nas_pdu_length = 0u;

    if (event_text == NULL) {
      return -1;
    }
    if (mini_gnb_c_extract_json_int(event_text, "abs_slot", &abs_slot) != 0) {
      free(event_text);
      return -1;
    }
    if (abs_slot > current_slot) {
      free(event_text);
      break;
    }
    if (mini_gnb_c_extract_json_int(event_text, "c_rnti", &c_rnti) != 0 ||
        mini_gnb_c_extract_json_string(event_text, "nas_hex", nas_hex, sizeof(nas_hex)) != 0 ||
        mini_gnb_c_hex_to_bytes(nas_hex, nas_pdu, sizeof(nas_pdu), &nas_pdu_length) != 0) {
      free(event_text);
      return -1;
    }
    if (mini_gnb_c_extract_json_int(event_text, "ran_ue_ngap_id", &ran_ue_ngap_id) != 0) {
      ran_ue_ngap_id = 0;
    }
    if (mini_gnb_c_extract_json_int(event_text, "amf_ue_ngap_id", &amf_ue_ngap_id) != 0) {
      amf_ue_ngap_id = 0;
    }
    /*
     * The UE observes this downlink JSON event only after the gNB has already
     * completed its UL_NAS poll for the same absolute slot. Start response
     * timing from the next slot so the emitted UL_NAS becomes visible on a
     * gNB poll instead of being stale immediately.
     */
    if (mini_gnb_c_nas_5gs_min_handle_downlink_nas(runtime,
                                                   (uint16_t)c_rnti,
                                                   (uint16_t)ran_ue_ngap_id,
                                                   ran_ue_ngap_id > 0,
                                                   (uint16_t)amf_ue_ngap_id,
                                                   amf_ue_ngap_id > 0,
                                                   nas_pdu,
                                                   nas_pdu_length,
                                                   abs_slot + 1) != 0) {
      free(event_text);
      return -1;
    }
    ++runtime->next_gnb_to_ue_sequence;
    free(event_text);
  }

  return mini_gnb_c_nas_5gs_min_emit_due_uplinks(runtime, exchange_dir, current_slot);
}
