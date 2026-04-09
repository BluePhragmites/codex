#include "mini_gnb_c/ngap/ngap_runtime.h"

#include <arpa/inet.h>
#include <string.h>

typedef struct {
  const uint8_t* bytes;
  size_t length;
} mini_gnb_c_ngap_octets_t;

typedef struct {
  uint16_t id;
  uint8_t criticality;
  mini_gnb_c_ngap_octets_t value;
} mini_gnb_c_ngap_ie_t;

typedef struct {
  const uint8_t* bytes;
  size_t length;
  uint32_t count;
  uint32_t index;
  size_t next_offset;
} mini_gnb_c_ngap_ie_sequence_t;

typedef struct {
  uint16_t id;
  uint8_t criticality;
  const uint8_t* value;
  size_t value_length;
} mini_gnb_c_ngap_build_ie_t;

static const uint8_t k_mini_gnb_c_ngap_user_location_information_nr[] = {
    0x40, 0x64, 0xf0, 0x99, 0x00, 0x06, 0x6c, 0x00, 0x00, 0x64, 0xf0, 0x99, 0x00, 0x00, 0x01,
};
static const uint8_t k_mini_gnb_c_ngap_ng_setup_request[] = {
    0x00u, 0x15u, 0x00u, 0x33u, 0x00u, 0x00u, 0x04u, 0x00u, 0x1bu, 0x00u, 0x08u, 0x00u, 0x64u,
    0xf0u, 0x99u, 0x00u, 0x00u, 0x06u, 0x6cu, 0x00u, 0x52u, 0x40u, 0x0au, 0x03u, 0x80u, 0x73u,
    0x72u, 0x73u, 0x67u, 0x6eu, 0x62u, 0x30u, 0x31u, 0x00u, 0x66u, 0x00u, 0x0du, 0x00u, 0x00u,
    0x00u, 0x00u, 0x01u, 0x00u, 0x64u, 0xf0u, 0x99u, 0x00u, 0x00u, 0x00u, 0x08u, 0x00u, 0x15u,
    0x40u, 0x01u, 0x60u,
};
static const uint8_t k_mini_gnb_c_ngap_rrc_establishment_cause_mo_signalling[] = {0x18u};
static const uint8_t k_mini_gnb_c_ngap_ue_context_request_requested[] = {0x00u};
static const uint8_t k_mini_gnb_c_ngap_pdu_session_resource_setup_response_list_template[] = {
    0x00, 0x00, 0x01, 0x0d, 0x00, 0x03, 0xe0, 0x7f, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x01,
};

static uint32_t mini_gnb_c_ngap_read_u16_be(const uint8_t* bytes) {
  return ((uint32_t)bytes[0] << 8u) | (uint32_t)bytes[1];
}

static uint32_t mini_gnb_c_ngap_read_u24_be(const uint8_t* bytes) {
  return ((uint32_t)bytes[0] << 16u) | ((uint32_t)bytes[1] << 8u) | (uint32_t)bytes[2];
}

static uint32_t mini_gnb_c_ngap_read_u32_be(const uint8_t* bytes) {
  return ((uint32_t)bytes[0] << 24u) | ((uint32_t)bytes[1] << 16u) | ((uint32_t)bytes[2] << 8u) | bytes[3];
}

static void mini_gnb_c_ngap_write_u16_be(uint16_t value, uint8_t out[2]) {
  out[0] = (uint8_t)(value >> 8u);
  out[1] = (uint8_t)(value & 0xffu);
}

static void mini_gnb_c_ngap_write_u32_be(uint32_t value, uint8_t out[4]) {
  out[0] = (uint8_t)(value >> 24u);
  out[1] = (uint8_t)((value >> 16u) & 0xffu);
  out[2] = (uint8_t)((value >> 8u) & 0xffu);
  out[3] = (uint8_t)(value & 0xffu);
}

static int mini_gnb_c_ngap_decode_compact_length(const uint8_t* bytes,
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

static int mini_gnb_c_ngap_encode_compact_length(uint8_t* bytes,
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

static int mini_gnb_c_ngap_build_octet_string_value(const uint8_t* octets,
                                                    size_t octets_length,
                                                    uint8_t* encoded_value,
                                                    size_t encoded_value_capacity,
                                                    size_t* encoded_value_length_out) {
  size_t length_field_size = 0u;

  if (octets == NULL || encoded_value == NULL || encoded_value_length_out == NULL) {
    return -1;
  }
  if (mini_gnb_c_ngap_encode_compact_length(encoded_value,
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

static int mini_gnb_c_ngap_build_pdu_session_resource_setup_response_list(const char* gnb_n3_ip,
                                                                          uint32_t gnb_n3_teid,
                                                                          uint8_t* encoded_value,
                                                                          size_t encoded_value_capacity,
                                                                          size_t* encoded_value_length_out) {
  struct in_addr gnb_addr;

  if (gnb_n3_ip == NULL || encoded_value == NULL || encoded_value_length_out == NULL ||
      encoded_value_capacity < sizeof(k_mini_gnb_c_ngap_pdu_session_resource_setup_response_list_template)) {
    return -1;
  }
  if (inet_pton(AF_INET, gnb_n3_ip, &gnb_addr) != 1) {
    return -1;
  }

  memcpy(encoded_value,
         k_mini_gnb_c_ngap_pdu_session_resource_setup_response_list_template,
         sizeof(k_mini_gnb_c_ngap_pdu_session_resource_setup_response_list_template));
  memcpy(encoded_value + 7u, &gnb_addr.s_addr, 4u);
  mini_gnb_c_ngap_write_u32_be(gnb_n3_teid, encoded_value + 11u);
  *encoded_value_length_out = sizeof(k_mini_gnb_c_ngap_pdu_session_resource_setup_response_list_template);
  return 0;
}

static int mini_gnb_c_ngap_extract_octet_string_value(const mini_gnb_c_ngap_octets_t* encoded_value,
                                                      uint8_t* octets_out,
                                                      size_t octets_capacity,
                                                      size_t* octets_length_out) {
  size_t octets_length = 0u;
  size_t length_field_size = 0u;

  if (encoded_value == NULL || octets_out == NULL || octets_length_out == NULL) {
    return -1;
  }
  if (mini_gnb_c_ngap_decode_compact_length(encoded_value->bytes,
                                            encoded_value->length,
                                            &octets_length,
                                            &length_field_size) != 0) {
    return -1;
  }
  if (length_field_size + octets_length > encoded_value->length || octets_length > octets_capacity) {
    return -1;
  }

  memcpy(octets_out, encoded_value->bytes + length_field_size, octets_length);
  *octets_length_out = octets_length;
  return 0;
}

static int mini_gnb_c_ngap_build_message(uint8_t pdu_type,
                                         uint8_t procedure_code,
                                         uint8_t criticality,
                                         const mini_gnb_c_ngap_build_ie_t* ies,
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
    if (mini_gnb_c_ngap_encode_compact_length(payload + payload_length,
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
  if (mini_gnb_c_ngap_encode_compact_length(out + 3u,
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

static int mini_gnb_c_ngap_ie_sequence_init_at(const uint8_t* bytes,
                                               size_t length,
                                               size_t count_offset,
                                               mini_gnb_c_ngap_ie_sequence_t* sequence) {
  if (bytes == NULL || sequence == NULL || count_offset + 3u > length) {
    return -1;
  }

  memset(sequence, 0, sizeof(*sequence));
  sequence->bytes = bytes;
  sequence->length = length;
  sequence->count = mini_gnb_c_ngap_read_u24_be(bytes + count_offset);
  if (sequence->count == 0u || sequence->count > 64u) {
    return -1;
  }
  sequence->next_offset = count_offset + 3u;
  return 0;
}

static int mini_gnb_c_ngap_ie_sequence_init_from_message(const uint8_t* bytes,
                                                         size_t length,
                                                         mini_gnb_c_ngap_ie_sequence_t* sequence) {
  size_t encoded_length = 0u;
  size_t length_field_bytes = 0u;
  size_t payload_offset = 0u;

  if (bytes == NULL || sequence == NULL || length < 7u) {
    return -1;
  }

  if (mini_gnb_c_ngap_decode_compact_length(bytes + 3u,
                                            length - 3u,
                                            &encoded_length,
                                            &length_field_bytes) != 0) {
    return -1;
  }

  payload_offset = 3u + length_field_bytes;
  if (payload_offset + encoded_length != length) {
    return -1;
  }

  return mini_gnb_c_ngap_ie_sequence_init_at(bytes, length, payload_offset, sequence);
}

static int mini_gnb_c_ngap_ie_sequence_next(mini_gnb_c_ngap_ie_sequence_t* sequence,
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

  if (mini_gnb_c_ngap_decode_compact_length(sequence->bytes + sequence->next_offset + 3u,
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

static int mini_gnb_c_ngap_find_ie_in_sequence(const mini_gnb_c_ngap_ie_sequence_t* sequence,
                                               uint16_t ie_id,
                                               mini_gnb_c_ngap_ie_t* ie_out) {
  mini_gnb_c_ngap_ie_sequence_t cursor;
  mini_gnb_c_ngap_ie_t ie;

  if (sequence == NULL || ie_out == NULL) {
    return -1;
  }

  cursor = *sequence;
  while (mini_gnb_c_ngap_ie_sequence_next(&cursor, &ie) == 0) {
    if (ie.id == ie_id) {
      *ie_out = ie;
      return 0;
    }
  }

  return -1;
}

static int mini_gnb_c_ngap_extract_ie(const uint8_t* bytes,
                                      size_t length,
                                      uint16_t ie_id,
                                      mini_gnb_c_ngap_ie_t* ie_out) {
  mini_gnb_c_ngap_ie_sequence_t sequence;

  if (ie_out == NULL) {
    return -1;
  }
  if (mini_gnb_c_ngap_ie_sequence_init_from_message(bytes, length, &sequence) != 0) {
    return -1;
  }
  return mini_gnb_c_ngap_find_ie_in_sequence(&sequence, ie_id, ie_out);
}

static int mini_gnb_c_ngap_find_tail_ie_sequence(const uint8_t* bytes,
                                                 size_t length,
                                                 const uint16_t* required_ids,
                                                 size_t required_id_count,
                                                 mini_gnb_c_ngap_ie_sequence_t* sequence_out) {
  size_t count_offset = 0u;
  int found = 0;

  if (bytes == NULL || sequence_out == NULL) {
    return -1;
  }

  for (count_offset = 0u; count_offset + 3u <= length; ++count_offset) {
    mini_gnb_c_ngap_ie_sequence_t sequence;
    mini_gnb_c_ngap_ie_sequence_t cursor;
    mini_gnb_c_ngap_ie_t ie;
    size_t required_hits = 0u;
    size_t id_index = 0u;

    if (mini_gnb_c_ngap_ie_sequence_init_at(bytes, length, count_offset, &sequence) != 0) {
      continue;
    }

    cursor = sequence;
    while (mini_gnb_c_ngap_ie_sequence_next(&cursor, &ie) == 0) {
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

static int mini_gnb_c_ngap_extract_pdu_session_setup_transfer_sequence(const uint8_t* bytes,
                                                                       size_t length,
                                                                       mini_gnb_c_ngap_ie_sequence_t* transfer_sequence) {
  static const uint16_t k_required_transfer_ies[] = {0x008bu, 0x0088u};
  mini_gnb_c_ngap_ie_t session_list_ie;

  if (transfer_sequence == NULL) {
    return -1;
  }
  if (mini_gnb_c_ngap_extract_ie(bytes, length, 0x004au, &session_list_ie) != 0) {
    return -1;
  }

  return mini_gnb_c_ngap_find_tail_ie_sequence(session_list_ie.value.bytes,
                                               session_list_ie.value.length,
                                               k_required_transfer_ies,
                                               sizeof(k_required_transfer_ies) / sizeof(k_required_transfer_ies[0]),
                                               transfer_sequence);
}

static const uint8_t* mini_gnb_c_ngap_find_nas_message(const uint8_t* bytes,
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

static int mini_gnb_c_ngap_extract_open5gs_upf_tunnel(const uint8_t* bytes,
                                                      size_t length,
                                                      mini_gnb_c_core_session_t* core_session) {
  mini_gnb_c_ngap_ie_sequence_t transfer_sequence;
  mini_gnb_c_ngap_ie_t gtp_tunnel_ie;
  struct in_addr upf_addr;
  char upf_ip[MINI_GNB_C_CORE_MAX_IPV4_TEXT];

  if (bytes == NULL || core_session == NULL) {
    return -1;
  }

  if (mini_gnb_c_ngap_extract_pdu_session_setup_transfer_sequence(bytes, length, &transfer_sequence) != 0 ||
      mini_gnb_c_ngap_find_ie_in_sequence(&transfer_sequence, 0x008bu, &gtp_tunnel_ie) != 0) {
    return -1;
  }
  if (gtp_tunnel_ie.value.length < 10u || gtp_tunnel_ie.value.bytes[1] != 0xf0u) {
    return -1;
  }

  memcpy(&upf_addr.s_addr, gtp_tunnel_ie.value.bytes + 2u, 4u);
  if (inet_ntop(AF_INET, &upf_addr, upf_ip, sizeof(upf_ip)) == NULL) {
    return -1;
  }

  return mini_gnb_c_core_session_set_upf_tunnel(core_session,
                                                upf_ip,
                                                mini_gnb_c_ngap_read_u32_be(gtp_tunnel_ie.value.bytes + 6u));
}

static int mini_gnb_c_ngap_extract_open5gs_qfi(const uint8_t* bytes,
                                               size_t length,
                                               mini_gnb_c_core_session_t* core_session) {
  mini_gnb_c_ngap_ie_sequence_t transfer_sequence;
  mini_gnb_c_ngap_ie_t qos_flow_ie;

  if (bytes == NULL || core_session == NULL) {
    return -1;
  }
  if (mini_gnb_c_ngap_extract_pdu_session_setup_transfer_sequence(bytes, length, &transfer_sequence) != 0 ||
      mini_gnb_c_ngap_find_ie_in_sequence(&transfer_sequence, 0x0088u, &qos_flow_ie) != 0) {
    return -1;
  }
  if (qos_flow_ie.value.length < 2u || qos_flow_ie.value.bytes[1] == 0u || qos_flow_ie.value.bytes[1] > 63u) {
    return -1;
  }

  return mini_gnb_c_core_session_set_qfi(core_session, qos_flow_ie.value.bytes[1]);
}

static int mini_gnb_c_ngap_extract_open5gs_ue_ipv4(const uint8_t* bytes,
                                                   size_t length,
                                                   mini_gnb_c_core_session_t* core_session) {
  mini_gnb_c_ngap_ie_t session_list_ie;
  const uint8_t* nas = NULL;
  size_t nas_offset = 0u;
  size_t index = 0u;

  if (bytes == NULL || core_session == NULL) {
    return -1;
  }
  if (mini_gnb_c_ngap_extract_ie(bytes, length, 0x004au, &session_list_ie) != 0) {
    return -1;
  }

  nas = mini_gnb_c_ngap_find_nas_message(session_list_ie.value.bytes,
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

int mini_gnb_c_ngap_build_ng_setup_request(uint8_t* message,
                                           size_t message_capacity,
                                           size_t* message_length) {
  if (message == NULL || message_length == NULL || message_capacity < sizeof(k_mini_gnb_c_ngap_ng_setup_request)) {
    return -1;
  }

  memcpy(message, k_mini_gnb_c_ngap_ng_setup_request, sizeof(k_mini_gnb_c_ngap_ng_setup_request));
  *message_length = sizeof(k_mini_gnb_c_ngap_ng_setup_request);
  return 0;
}

int mini_gnb_c_ngap_build_initial_ue_message(const uint8_t* nas_pdu,
                                             size_t nas_pdu_length,
                                             uint16_t ran_ue_ngap_id,
                                             uint8_t* message,
                                             size_t message_capacity,
                                             size_t* message_length) {
  uint8_t ran_ue_ngap_id_bytes[2];
  uint8_t nas_ie_value[512];
  size_t nas_ie_value_length = 0u;
  const mini_gnb_c_ngap_build_ie_t ies[] = {
      {0x0055u, 0x00u, ran_ue_ngap_id_bytes, sizeof(ran_ue_ngap_id_bytes)},
      {0x0026u, 0x00u, nas_ie_value, 0u},
      {0x0079u, 0x00u, k_mini_gnb_c_ngap_user_location_information_nr,
       sizeof(k_mini_gnb_c_ngap_user_location_information_nr)},
      {0x005au, 0x40u, k_mini_gnb_c_ngap_rrc_establishment_cause_mo_signalling,
       sizeof(k_mini_gnb_c_ngap_rrc_establishment_cause_mo_signalling)},
      {0x0070u, 0x40u, k_mini_gnb_c_ngap_ue_context_request_requested,
       sizeof(k_mini_gnb_c_ngap_ue_context_request_requested)},
  };
  mini_gnb_c_ngap_build_ie_t working_ies[sizeof(ies) / sizeof(ies[0])];

  if (nas_pdu == NULL || message == NULL || message_length == NULL || nas_pdu_length == 0u) {
    return -1;
  }

  mini_gnb_c_ngap_write_u16_be(ran_ue_ngap_id, ran_ue_ngap_id_bytes);
  if (mini_gnb_c_ngap_build_octet_string_value(nas_pdu,
                                               nas_pdu_length,
                                               nas_ie_value,
                                               sizeof(nas_ie_value),
                                               &nas_ie_value_length) != 0) {
    return -1;
  }

  memcpy(working_ies, ies, sizeof(working_ies));
  working_ies[1].value_length = nas_ie_value_length;
  return mini_gnb_c_ngap_build_message(0x00u,
                                       0x0fu,
                                       0x40u,
                                       working_ies,
                                       sizeof(working_ies) / sizeof(working_ies[0]),
                                       message,
                                       message_capacity,
                                       message_length);
}

int mini_gnb_c_ngap_build_uplink_nas_transport(uint16_t amf_ue_ngap_id,
                                               uint16_t ran_ue_ngap_id,
                                               const uint8_t* nas_pdu,
                                               size_t nas_pdu_length,
                                               uint8_t* message,
                                               size_t message_capacity,
                                               size_t* message_length) {
  uint8_t amf_ue_ngap_id_bytes[2];
  uint8_t ran_ue_ngap_id_bytes[2];
  uint8_t nas_ie_value[512];
  size_t nas_ie_value_length = 0u;
  const mini_gnb_c_ngap_build_ie_t ies[] = {
      {0x000au, 0x00u, amf_ue_ngap_id_bytes, sizeof(amf_ue_ngap_id_bytes)},
      {0x0055u, 0x00u, ran_ue_ngap_id_bytes, sizeof(ran_ue_ngap_id_bytes)},
      {0x0026u, 0x00u, nas_ie_value, 0u},
      {0x0079u, 0x40u, k_mini_gnb_c_ngap_user_location_information_nr,
       sizeof(k_mini_gnb_c_ngap_user_location_information_nr)},
  };
  mini_gnb_c_ngap_build_ie_t working_ies[sizeof(ies) / sizeof(ies[0])];

  if (nas_pdu == NULL || message == NULL || message_length == NULL || nas_pdu_length == 0u) {
    return -1;
  }

  mini_gnb_c_ngap_write_u16_be(amf_ue_ngap_id, amf_ue_ngap_id_bytes);
  mini_gnb_c_ngap_write_u16_be(ran_ue_ngap_id, ran_ue_ngap_id_bytes);
  if (mini_gnb_c_ngap_build_octet_string_value(nas_pdu,
                                               nas_pdu_length,
                                               nas_ie_value,
                                               sizeof(nas_ie_value),
                                               &nas_ie_value_length) != 0) {
    return -1;
  }

  memcpy(working_ies, ies, sizeof(working_ies));
  working_ies[2].value_length = nas_ie_value_length;
  return mini_gnb_c_ngap_build_message(0x00u,
                                       0x2eu,
                                       0x40u,
                                       working_ies,
                                       sizeof(working_ies) / sizeof(working_ies[0]),
                                       message,
                                       message_capacity,
                                       message_length);
}

int mini_gnb_c_ngap_build_downlink_nas_transport(uint16_t amf_ue_ngap_id,
                                                 uint16_t ran_ue_ngap_id,
                                                 const uint8_t* nas_pdu,
                                                 size_t nas_pdu_length,
                                                 uint8_t* message,
                                                 size_t message_capacity,
                                                 size_t* message_length) {
  uint8_t amf_ue_ngap_id_bytes[2];
  uint8_t ran_ue_ngap_id_bytes[2];
  uint8_t nas_ie_value[512];
  size_t nas_ie_value_length = 0u;
  const mini_gnb_c_ngap_build_ie_t ies[] = {
      {0x000au, 0x00u, amf_ue_ngap_id_bytes, sizeof(amf_ue_ngap_id_bytes)},
      {0x0055u, 0x00u, ran_ue_ngap_id_bytes, sizeof(ran_ue_ngap_id_bytes)},
      {0x0026u, 0x00u, nas_ie_value, 0u},
  };
  mini_gnb_c_ngap_build_ie_t working_ies[sizeof(ies) / sizeof(ies[0])];

  if (nas_pdu == NULL || message == NULL || message_length == NULL || nas_pdu_length == 0u) {
    return -1;
  }

  mini_gnb_c_ngap_write_u16_be(amf_ue_ngap_id, amf_ue_ngap_id_bytes);
  mini_gnb_c_ngap_write_u16_be(ran_ue_ngap_id, ran_ue_ngap_id_bytes);
  if (mini_gnb_c_ngap_build_octet_string_value(nas_pdu,
                                               nas_pdu_length,
                                               nas_ie_value,
                                               sizeof(nas_ie_value),
                                               &nas_ie_value_length) != 0) {
    return -1;
  }

  memcpy(working_ies, ies, sizeof(working_ies));
  working_ies[2].value_length = nas_ie_value_length;
  return mini_gnb_c_ngap_build_message(0x00u,
                                       0x04u,
                                       0x00u,
                                       working_ies,
                                       sizeof(working_ies) / sizeof(working_ies[0]),
                                       message,
                                       message_capacity,
                                       message_length);
}

int mini_gnb_c_ngap_build_initial_context_setup_response(uint16_t amf_ue_ngap_id,
                                                         uint16_t ran_ue_ngap_id,
                                                         uint8_t* message,
                                                         size_t message_capacity,
                                                         size_t* message_length) {
  uint8_t amf_ue_ngap_id_bytes[2];
  uint8_t ran_ue_ngap_id_bytes[2];
  const mini_gnb_c_ngap_build_ie_t ies[] = {
      {0x000au, 0x40u, amf_ue_ngap_id_bytes, sizeof(amf_ue_ngap_id_bytes)},
      {0x0055u, 0x40u, ran_ue_ngap_id_bytes, sizeof(ran_ue_ngap_id_bytes)},
  };

  if (message == NULL || message_length == NULL) {
    return -1;
  }

  mini_gnb_c_ngap_write_u16_be(amf_ue_ngap_id, amf_ue_ngap_id_bytes);
  mini_gnb_c_ngap_write_u16_be(ran_ue_ngap_id, ran_ue_ngap_id_bytes);
  return mini_gnb_c_ngap_build_message(0x20u,
                                       0x0eu,
                                       0x00u,
                                       ies,
                                       sizeof(ies) / sizeof(ies[0]),
                                       message,
                                       message_capacity,
                                       message_length);
}

int mini_gnb_c_ngap_build_pdu_session_resource_setup_response(uint16_t amf_ue_ngap_id,
                                                              uint16_t ran_ue_ngap_id,
                                                              uint8_t* message,
                                                              size_t message_capacity,
                                                              size_t* message_length) {
  return mini_gnb_c_ngap_build_pdu_session_resource_setup_response_with_tunnel(amf_ue_ngap_id,
                                                                               ran_ue_ngap_id,
                                                                               "127.0.0.1",
                                                                               0x00000001u,
                                                                               message,
                                                                               message_capacity,
                                                                               message_length);
}

int mini_gnb_c_ngap_build_pdu_session_resource_setup_response_with_tunnel(uint16_t amf_ue_ngap_id,
                                                                          uint16_t ran_ue_ngap_id,
                                                                          const char* gnb_n3_ip,
                                                                          uint32_t gnb_n3_teid,
                                                                          uint8_t* message,
                                                                          size_t message_capacity,
                                                                          size_t* message_length) {
  uint8_t amf_ue_ngap_id_bytes[2];
  uint8_t ran_ue_ngap_id_bytes[2];
  uint8_t response_list[64];
  size_t response_list_length = 0u;
  const mini_gnb_c_ngap_build_ie_t ies[] = {
      {0x000au, 0x40u, amf_ue_ngap_id_bytes, sizeof(amf_ue_ngap_id_bytes)},
      {0x0055u, 0x40u, ran_ue_ngap_id_bytes, sizeof(ran_ue_ngap_id_bytes)},
      {0x004bu, 0x40u, response_list, 0u},
  };
  mini_gnb_c_ngap_build_ie_t working_ies[sizeof(ies) / sizeof(ies[0])];

  if (message == NULL || message_length == NULL || gnb_n3_ip == NULL) {
    return -1;
  }
  if (mini_gnb_c_ngap_build_pdu_session_resource_setup_response_list(gnb_n3_ip,
                                                                     gnb_n3_teid,
                                                                     response_list,
                                                                     sizeof(response_list),
                                                                     &response_list_length) != 0) {
    return -1;
  }

  mini_gnb_c_ngap_write_u16_be(amf_ue_ngap_id, amf_ue_ngap_id_bytes);
  mini_gnb_c_ngap_write_u16_be(ran_ue_ngap_id, ran_ue_ngap_id_bytes);
  memcpy(working_ies, ies, sizeof(working_ies));
  working_ies[2].value_length = response_list_length;
  return mini_gnb_c_ngap_build_message(0x20u,
                                       0x1du,
                                       0x00u,
                                       working_ies,
                                       sizeof(working_ies) / sizeof(working_ies[0]),
                                       message,
                                       message_capacity,
                                       message_length);
}

int mini_gnb_c_ngap_extract_amf_ue_ngap_id(const uint8_t* message,
                                           size_t message_length,
                                           uint16_t* amf_ue_ngap_id_out) {
  mini_gnb_c_ngap_ie_t ie;

  if (amf_ue_ngap_id_out == NULL) {
    return -1;
  }
  if (mini_gnb_c_ngap_extract_ie(message, message_length, 0x000au, &ie) != 0 || ie.value.length != 2u) {
    return -1;
  }

  *amf_ue_ngap_id_out = (uint16_t)mini_gnb_c_ngap_read_u16_be(ie.value.bytes);
  return 0;
}

int mini_gnb_c_ngap_extract_nas_pdu(const uint8_t* message,
                                    size_t message_length,
                                    uint8_t* nas_pdu_out,
                                    size_t nas_pdu_capacity,
                                    size_t* nas_pdu_length_out) {
  mini_gnb_c_ngap_ie_t ie;

  if (nas_pdu_length_out == NULL) {
    return -1;
  }
  if (mini_gnb_c_ngap_extract_ie(message, message_length, 0x0026u, &ie) != 0) {
    return -1;
  }

  return mini_gnb_c_ngap_extract_octet_string_value(&ie.value,
                                                    nas_pdu_out,
                                                    nas_pdu_capacity,
                                                    nas_pdu_length_out);
}

int mini_gnb_c_ngap_extract_open5gs_user_plane_state(const uint8_t* message,
                                                     size_t message_length,
                                                     mini_gnb_c_core_session_t* core_session) {
  if (message == NULL || core_session == NULL) {
    return -1;
  }

  if (mini_gnb_c_ngap_extract_open5gs_upf_tunnel(message, message_length, core_session) != 0) {
    return -1;
  }
  if (mini_gnb_c_ngap_extract_open5gs_qfi(message, message_length, core_session) != 0) {
    return -1;
  }
  if (mini_gnb_c_ngap_extract_open5gs_ue_ipv4(message, message_length, core_session) != 0) {
    return -1;
  }

  return 0;
}
