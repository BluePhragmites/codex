#include "mini_gnb_c/common/simulator.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mini_gnb_c/common/hex.h"
#include "mini_gnb_c/common/json_utils.h"

static const char* mini_gnb_c_bool_string(bool value) {
  return value ? "true" : "false";
}

static void mini_gnb_c_join_lcid_sequence(const mini_gnb_c_mac_ul_parse_result_t* mac_result,
                                          char* out,
                                          size_t out_size) {
  size_t i = 0;
  size_t offset = 0;

  if (out == NULL || out_size == 0U) {
    return;
  }

  out[0] = '\0';
  if (mac_result == NULL) {
    return;
  }

  for (i = 0; i < mac_result->lcid_count; ++i) {
    int written = snprintf(out + offset,
                           out_size - offset,
                           "%s%d",
                           (i == 0U) ? "" : "/",
                           mac_result->lcid_sequence[i]);
    if (written < 0 || (size_t)written >= (out_size - offset)) {
      break;
    }
    offset += (size_t)written;
  }
}

static mini_gnb_c_ue_context_t* mini_gnb_c_find_ue_context(mini_gnb_c_ue_context_store_t* store, uint16_t rnti) {
  if (store == NULL || store->count == 0U) {
    return NULL;
  }
  if (store->contexts[0].c_rnti == rnti || store->contexts[0].tc_rnti == rnti) {
    return &store->contexts[0];
  }
  return NULL;
}

static mini_gnb_c_ue_context_t* mini_gnb_c_find_expected_connected_ue(mini_gnb_c_simulator_t* simulator,
                                                                      uint16_t rnti) {
  if (simulator == NULL || simulator->ue_store.count == 0U) {
    return NULL;
  }
  if (rnti != 0U) {
    return mini_gnb_c_find_ue_context(&simulator->ue_store, rnti);
  }
  return &simulator->ue_store.contexts[0];
}

static const char* mini_gnb_c_ul_data_purpose_string(const mini_gnb_c_ul_data_purpose_t purpose) {
  return purpose == MINI_GNB_C_UL_DATA_PURPOSE_BSR ? "BSR" : "PAYLOAD";
}

static int mini_gnb_c_parse_bsr_buffer_size_bytes(const mini_gnb_c_buffer_t* mac_pdu) {
  char text[MINI_GNB_C_MAX_PAYLOAD + 1U];
  int buffer_size_bytes = -1;

  if (mac_pdu == NULL || mac_pdu->len == 0U || mac_pdu->len > MINI_GNB_C_MAX_PAYLOAD) {
    return -1;
  }

  memcpy(text, mac_pdu->bytes, mac_pdu->len);
  text[mac_pdu->len] = '\0';
  return (sscanf(text, "BSR|bytes=%d", &buffer_size_bytes) == 1 && buffer_size_bytes >= 0) ? buffer_size_bytes : -1;
}

static uint16_t mini_gnb_c_select_large_ul_prb_len(const int buffer_size_bytes) {
  int prb_len = ((buffer_size_bytes + 63) / 64) * 4;

  if (prb_len < 16) {
    prb_len = 16;
  }
  if (prb_len > 40) {
    prb_len = 40;
  }
  return (uint16_t)prb_len;
}

static bool mini_gnb_c_path_is_absolute(const char* path) {
  if (path == NULL || path[0] == '\0') {
    return false;
  }
  if (path[0] == '/' || path[0] == '\\') {
    return true;
  }
  return strlen(path) > 1U && path[1] == ':';
}

static void mini_gnb_c_resolve_optional_dir_in_place(char* path, const size_t path_size) {
  char resolved[MINI_GNB_C_MAX_PATH];

  if (path == NULL || path_size == 0U || path[0] == '\0' || mini_gnb_c_path_is_absolute(path)) {
    return;
  }

  if (mini_gnb_c_join_path(MINI_GNB_C_SOURCE_DIR, path, resolved, sizeof(resolved)) == 0) {
    (void)snprintf(path, path_size, "%s", resolved);
  }
}

static char* mini_gnb_c_ltrim(char* text) {
  if (text == NULL) {
    return NULL;
  }
  while (*text != '\0' && (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')) {
    ++text;
  }
  return text;
}

static void mini_gnb_c_rtrim(char* text) {
  size_t len = 0;

  if (text == NULL) {
    return;
  }

  len = strlen(text);
  while (len > 0U &&
         (text[len - 1U] == ' ' || text[len - 1U] == '\t' || text[len - 1U] == '\r' || text[len - 1U] == '\n')) {
    text[len - 1U] = '\0';
    --len;
  }
}

static void mini_gnb_c_unquote(char* text) {
  size_t len = 0;

  if (text == NULL) {
    return;
  }

  len = strlen(text);
  if (len >= 2U &&
      ((text[0] == '"' && text[len - 1U] == '"') || (text[0] == '\'' && text[len - 1U] == '\''))) {
    memmove(text, text + 1, len - 2U);
    text[len - 2U] = '\0';
  }
}

static int mini_gnb_c_extract_transport_value(const char* text,
                                              const char* key,
                                              char* out,
                                              size_t out_size) {
  const char* cursor = text;

  if (text == NULL || key == NULL || out == NULL || out_size == 0U) {
    return -1;
  }

  while (*cursor != '\0') {
    char line[256];
    size_t len = 0;
    char* trimmed = NULL;
    char* separator = NULL;
    char* value = NULL;

    while (cursor[len] != '\0' && cursor[len] != '\n' && cursor[len] != '\r') {
      ++len;
    }
    if (len >= sizeof(line)) {
      len = sizeof(line) - 1U;
    }
    memcpy(line, cursor, len);
    line[len] = '\0';

    cursor += len;
    while (*cursor == '\n' || *cursor == '\r') {
      ++cursor;
    }

    trimmed = mini_gnb_c_ltrim(line);
    mini_gnb_c_rtrim(trimmed);
    if (*trimmed == '\0' || *trimmed == '#') {
      continue;
    }

    separator = strchr(trimmed, '=');
    if (separator == NULL) {
      separator = strchr(trimmed, ':');
    }
    if (separator == NULL) {
      continue;
    }

    *separator = '\0';
    mini_gnb_c_rtrim(trimmed);
    value = mini_gnb_c_ltrim(separator + 1);
    mini_gnb_c_rtrim(value);
    mini_gnb_c_unquote(value);

    if (strcmp(trimmed, key) != 0) {
      continue;
    }

    return snprintf(out, out_size, "%s", value) < (int)out_size ? 0 : -1;
  }

  return -1;
}

static int mini_gnb_c_extract_transport_value_any(const char* text,
                                                  const char* first_key,
                                                  const char* second_key,
                                                  char* out,
                                                  size_t out_size) {
  if (mini_gnb_c_extract_transport_value(text, first_key, out, out_size) == 0) {
    return 0;
  }
  if (second_key != NULL && second_key[0] != '\0') {
    return mini_gnb_c_extract_transport_value(text, second_key, out, out_size);
  }
  return -1;
}

static int mini_gnb_c_extract_transport_int(const char* text, const char* key, int* out) {
  char value_text[64];
  char* end_ptr = NULL;

  if (out == NULL || mini_gnb_c_extract_transport_value(text, key, value_text, sizeof(value_text)) != 0) {
    return -1;
  }

  *out = (int)strtol(value_text, &end_ptr, 10);
  end_ptr = mini_gnb_c_ltrim(end_ptr);
  return (end_ptr != value_text && end_ptr != NULL && *end_ptr == '\0') ? 0 : -1;
}

static int mini_gnb_c_build_script_path(const char* dir,
                                        const int abs_slot,
                                        const char* name,
                                        char* out,
                                        const size_t out_size) {
  if (dir == NULL || dir[0] == '\0' || name == NULL || out == NULL || out_size == 0U) {
    return -1;
  }

  return snprintf(out, out_size, "%s/slot_%d_%s.txt", dir, abs_slot, name) < (int)out_size ? 0 : -1;
}

static void mini_gnb_c_fill_payload_from_transport(const char* text,
                                                   const char* default_payload_text,
                                                   mini_gnb_c_buffer_t* out_payload) {
  char payload_hex[MINI_GNB_C_MAX_PAYLOAD * 2U + 1U];
  char payload_text[MINI_GNB_C_MAX_PAYLOAD + 1U];
  size_t payload_len = 0U;

  if (text == NULL || out_payload == NULL) {
    return;
  }

  mini_gnb_c_buffer_reset(out_payload);
  if (mini_gnb_c_extract_transport_value_any(text, "payload_hex", "mac_pdu_hex", payload_hex, sizeof(payload_hex)) ==
          0 &&
      mini_gnb_c_hex_to_bytes(payload_hex,
                              out_payload->bytes,
                              sizeof(out_payload->bytes),
                              &payload_len) == 0) {
    out_payload->len = payload_len;
    return;
  }

  if (mini_gnb_c_extract_transport_value(text, "payload_text", payload_text, sizeof(payload_text)) == 0) {
    (void)mini_gnb_c_buffer_set_text(out_payload, payload_text);
    return;
  }

  if (default_payload_text != NULL && default_payload_text[0] != '\0') {
    (void)mini_gnb_c_buffer_set_text(out_payload, default_payload_text);
  }
}

static mini_gnb_c_dci_format_t mini_gnb_c_parse_dci_format(const char* text,
                                                           const char* key,
                                                           const mini_gnb_c_dci_format_t fallback) {
  char value[16];

  if (mini_gnb_c_extract_transport_value(text, key, value, sizeof(value)) != 0) {
    return fallback;
  }

  if (strcmp(value, "DCI1_0") == 0) {
    return MINI_GNB_C_DCI_FORMAT_1_0;
  }
  if (strcmp(value, "DCI1_1") == 0) {
    return MINI_GNB_C_DCI_FORMAT_1_1;
  }
  if (strcmp(value, "DCI0_0") == 0) {
    return MINI_GNB_C_DCI_FORMAT_0_0;
  }
  if (strcmp(value, "DCI0_1") == 0) {
    return MINI_GNB_C_DCI_FORMAT_0_1;
  }

  return fallback;
}

static mini_gnb_c_ul_data_purpose_t mini_gnb_c_parse_ul_purpose(const char* text,
                                                                 const mini_gnb_c_ul_data_purpose_t fallback) {
  char value[32];

  if (mini_gnb_c_extract_transport_value_any(text, "purpose", "scheduled_purpose", value, sizeof(value)) != 0 &&
      mini_gnb_c_extract_transport_value(text, "scheduled_type", value, sizeof(value)) != 0 &&
      mini_gnb_c_extract_transport_value(text, "type", value, sizeof(value)) != 0) {
    return fallback;
  }

  if (strcmp(value, "BSR") == 0 || strcmp(value, "SCRIPT_UL_BSR") == 0) {
    return MINI_GNB_C_UL_DATA_PURPOSE_BSR;
  }
  return MINI_GNB_C_UL_DATA_PURPOSE_PAYLOAD;
}

static uint16_t mini_gnb_c_resolve_script_rnti(mini_gnb_c_simulator_t* simulator, const char* text) {
  int int_value = 0;
  mini_gnb_c_ue_context_t* ue_context = NULL;

  if (mini_gnb_c_extract_transport_int(text, "rnti", &int_value) == 0 && int_value > 0) {
    return (uint16_t)int_value;
  }

  ue_context = mini_gnb_c_find_expected_connected_ue(simulator, 0U);
  return ue_context != NULL ? ue_context->c_rnti : 0U;
}

static void mini_gnb_c_mark_scripted_dl_plan(mini_gnb_c_simulator_t* simulator,
                                             const mini_gnb_c_dl_data_schedule_request_t* request) {
  mini_gnb_c_ue_context_t* ue_context = NULL;

  if (simulator == NULL || request == NULL) {
    return;
  }

  ue_context = mini_gnb_c_find_ue_context(&simulator->ue_store, request->c_rnti);
  if (ue_context == NULL) {
    return;
  }

  ue_context->traffic_plan_scheduled = true;
  ue_context->dl_data_abs_slot = request->abs_slot;
}

static void mini_gnb_c_mark_scripted_ul_plan(mini_gnb_c_simulator_t* simulator,
                                             const mini_gnb_c_ul_data_schedule_request_t* request) {
  mini_gnb_c_ue_context_t* ue_context = NULL;

  if (simulator == NULL || request == NULL) {
    return;
  }

  ue_context = mini_gnb_c_find_ue_context(&simulator->ue_store, request->c_rnti);
  if (ue_context == NULL) {
    return;
  }

  ue_context->traffic_plan_scheduled = true;
  if (request->purpose == MINI_GNB_C_UL_DATA_PURPOSE_BSR) {
    ue_context->small_ul_grant_abs_slot = request->abs_slot;
  } else {
    ue_context->large_ul_grant_abs_slot = request->abs_slot;
  }
}

static void mini_gnb_c_arm_scripted_ul_data(mini_gnb_c_simulator_t* simulator,
                                            const mini_gnb_c_ul_data_schedule_request_t* request) {
  mini_gnb_c_ul_data_grant_t armed_ul_grant;

  if (simulator == NULL || request == NULL) {
    return;
  }

  memset(&armed_ul_grant, 0, sizeof(armed_ul_grant));
  armed_ul_grant.c_rnti = request->c_rnti;
  armed_ul_grant.abs_slot = request->abs_slot;
  armed_ul_grant.prb_start = request->prb_start;
  armed_ul_grant.prb_len = request->prb_len;
  armed_ul_grant.mcs = request->mcs;
  armed_ul_grant.k2 = request->k2;
  armed_ul_grant.purpose = request->purpose;
  mini_gnb_c_mock_radio_frontend_arm_ul_data(&simulator->radio, &armed_ul_grant);
}

static void mini_gnb_c_process_scripted_dl_schedule(mini_gnb_c_simulator_t* simulator,
                                                    const mini_gnb_c_slot_indication_t* slot) {
  char path[MINI_GNB_C_MAX_PATH];
  char default_payload[MINI_GNB_C_MAX_TEXT];
  char type_value[32];
  char* text = NULL;
  mini_gnb_c_dl_data_schedule_request_t request;
  int int_value = 0;
  uint16_t rnti = 0U;

  if (simulator == NULL || slot == NULL || simulator->config.sim.scripted_schedule_dir[0] == '\0' ||
      mini_gnb_c_build_script_path(simulator->config.sim.scripted_schedule_dir,
                                   slot->abs_slot,
                                   "SCRIPT_DL",
                                   path,
                                   sizeof(path)) != 0) {
    return;
  }

  text = mini_gnb_c_read_text_file(path);
  if (text == NULL) {
    return;
  }

  if (mini_gnb_c_extract_transport_value(text, "type", type_value, sizeof(type_value)) == 0 &&
      strcmp(type_value, "SCRIPT_DL_DATA") != 0 && strcmp(type_value, "DL_DATA") != 0) {
    free(text);
    return;
  }

  memset(&request, 0, sizeof(request));
  rnti = mini_gnb_c_resolve_script_rnti(simulator, text);
  if (rnti == 0U) {
    free(text);
    return;
  }

  request.c_rnti = rnti;
  request.abs_slot = slot->abs_slot;
  request.prb_start = 52U;
  request.prb_len = 24U;
  request.mcs = 9U;
  request.dci_format = MINI_GNB_C_DCI_FORMAT_1_1;

  if (mini_gnb_c_extract_transport_int(text, "abs_slot", &int_value) == 0) {
    request.abs_slot = int_value;
  }
  if (mini_gnb_c_extract_transport_int(text, "prb_start", &int_value) == 0 && int_value >= 0) {
    request.prb_start = (uint16_t)int_value;
  }
  if (mini_gnb_c_extract_transport_int(text, "prb_len", &int_value) == 0 && int_value > 0) {
    request.prb_len = (uint16_t)int_value;
  }
  if (mini_gnb_c_extract_transport_int(text, "mcs", &int_value) == 0 && int_value >= 0) {
    request.mcs = (uint8_t)int_value;
  }
  request.dci_format = mini_gnb_c_parse_dci_format(text, "dci_format", request.dci_format);
  if (snprintf(default_payload,
               sizeof(default_payload),
               "SCRIPTED_DL|abs_slot=%d|src=direct",
               request.abs_slot) < (int)sizeof(default_payload)) {
    mini_gnb_c_fill_payload_from_transport(text, default_payload, &request.payload);
  }

  mini_gnb_c_initial_access_scheduler_queue_dl_data(&simulator->scheduler, &request, &simulator->metrics);
  mini_gnb_c_mark_scripted_dl_plan(simulator, &request);
  mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                 "scripted_schedule",
                                 "Queued direct scripted DL schedule.",
                                 slot->abs_slot,
                                 "rnti=%u,dl_abs_slot=%d,prb_start=%u,prb_len=%u,mcs=%u,dci=%s,path=%s",
                                 request.c_rnti,
                                 request.abs_slot,
                                 request.prb_start,
                                 request.prb_len,
                                 request.mcs,
                                 mini_gnb_c_dci_format_to_string(request.dci_format),
                                 path);
  free(text);
}

static void mini_gnb_c_process_scripted_ul_schedule(mini_gnb_c_simulator_t* simulator,
                                                    const mini_gnb_c_slot_indication_t* slot) {
  char path[MINI_GNB_C_MAX_PATH];
  char type_value[32];
  char* text = NULL;
  mini_gnb_c_ul_data_schedule_request_t request;
  int int_value = 0;
  uint16_t rnti = 0U;

  if (simulator == NULL || slot == NULL || simulator->config.sim.scripted_schedule_dir[0] == '\0' ||
      mini_gnb_c_build_script_path(simulator->config.sim.scripted_schedule_dir,
                                   slot->abs_slot,
                                   "SCRIPT_UL",
                                   path,
                                   sizeof(path)) != 0) {
    return;
  }

  text = mini_gnb_c_read_text_file(path);
  if (text == NULL) {
    return;
  }

  if (mini_gnb_c_extract_transport_value(text, "type", type_value, sizeof(type_value)) == 0 &&
      strcmp(type_value, "SCRIPT_UL_GRANT") != 0 && strcmp(type_value, "UL_GRANT") != 0 &&
      strcmp(type_value, "SCRIPT_UL_BSR") != 0) {
    free(text);
    return;
  }

  memset(&request, 0, sizeof(request));
  rnti = mini_gnb_c_resolve_script_rnti(simulator, text);
  if (rnti == 0U) {
    free(text);
    return;
  }

  request.c_rnti = rnti;
  request.pdcch_abs_slot = slot->abs_slot;
  request.k2 = 2U;
  request.purpose = mini_gnb_c_parse_ul_purpose(text, MINI_GNB_C_UL_DATA_PURPOSE_PAYLOAD);
  request.prb_start = request.purpose == MINI_GNB_C_UL_DATA_PURPOSE_BSR ? 60U : 48U;
  request.prb_len = request.purpose == MINI_GNB_C_UL_DATA_PURPOSE_BSR ? 8U : 24U;
  request.mcs = request.purpose == MINI_GNB_C_UL_DATA_PURPOSE_BSR ? 4U : 8U;
  if (mini_gnb_c_extract_transport_int(text, "k2", &int_value) == 0 && int_value >= 0) {
    request.k2 = (uint8_t)int_value;
  }
  request.abs_slot = request.pdcch_abs_slot + request.k2;
  if (mini_gnb_c_extract_transport_int(text, "abs_slot", &int_value) == 0) {
    request.abs_slot = int_value;
  } else if (mini_gnb_c_extract_transport_int(text, "scheduled_abs_slot", &int_value) == 0) {
    request.abs_slot = int_value;
  }
  if (mini_gnb_c_extract_transport_int(text, "prb_start", &int_value) == 0 && int_value >= 0) {
    request.prb_start = (uint16_t)int_value;
  } else if (mini_gnb_c_extract_transport_int(text, "scheduled_prb_start", &int_value) == 0 && int_value >= 0) {
    request.prb_start = (uint16_t)int_value;
  }
  if (mini_gnb_c_extract_transport_int(text, "prb_len", &int_value) == 0 && int_value > 0) {
    request.prb_len = (uint16_t)int_value;
  } else if (mini_gnb_c_extract_transport_int(text, "scheduled_prb_len", &int_value) == 0 && int_value > 0) {
    request.prb_len = (uint16_t)int_value;
  }
  if (mini_gnb_c_extract_transport_int(text, "mcs", &int_value) == 0 && int_value >= 0) {
    request.mcs = (uint8_t)int_value;
  }

  mini_gnb_c_initial_access_scheduler_queue_ul_data_grant(&simulator->scheduler, &request, &simulator->metrics);
  mini_gnb_c_arm_scripted_ul_data(simulator, &request);
  mini_gnb_c_mark_scripted_ul_plan(simulator, &request);
  mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                 "scripted_schedule",
                                 "Queued direct scripted UL schedule.",
                                 slot->abs_slot,
                                 "rnti=%u,purpose=%s,ul_abs_slot=%d,prb_start=%u,prb_len=%u,mcs=%u,k2=%u,path=%s",
                                 request.c_rnti,
                                 mini_gnb_c_ul_data_purpose_string(request.purpose),
                                 request.abs_slot,
                                 request.prb_start,
                                 request.prb_len,
                                 request.mcs,
                                 request.k2,
                                 path);
  free(text);
}

static void mini_gnb_c_process_scripted_pdcch_dl(mini_gnb_c_simulator_t* simulator,
                                                 const mini_gnb_c_slot_indication_t* slot) {
  char path[MINI_GNB_C_MAX_PATH];
  char default_payload[MINI_GNB_C_MAX_TEXT];
  char scheduled_type[32];
  char* text = NULL;
  mini_gnb_c_dl_data_schedule_request_t request;
  int int_value = 0;
  uint16_t rnti = 0U;

  if (simulator == NULL || slot == NULL || simulator->config.sim.scripted_pdcch_dir[0] == '\0' ||
      mini_gnb_c_build_script_path(simulator->config.sim.scripted_pdcch_dir,
                                   slot->abs_slot,
                                   "SCRIPT_PDCCH_DL",
                                   path,
                                   sizeof(path)) != 0) {
    return;
  }

  text = mini_gnb_c_read_text_file(path);
  if (text == NULL) {
    return;
  }

  if (mini_gnb_c_extract_transport_value(text, "scheduled_type", scheduled_type, sizeof(scheduled_type)) == 0 &&
      strcmp(scheduled_type, "DATA") != 0 && strcmp(scheduled_type, "DL_OBJ_DATA") != 0) {
    free(text);
    return;
  }

  memset(&request, 0, sizeof(request));
  rnti = mini_gnb_c_resolve_script_rnti(simulator, text);
  if (rnti == 0U) {
    free(text);
    return;
  }

  request.c_rnti = rnti;
  request.abs_slot = slot->abs_slot;
  request.prb_start = 52U;
  request.prb_len = 24U;
  request.mcs = 9U;
  request.dci_format = mini_gnb_c_parse_dci_format(text, "dci_format", MINI_GNB_C_DCI_FORMAT_1_1);

  if (mini_gnb_c_extract_transport_int(text, "scheduled_abs_slot", &int_value) == 0) {
    request.abs_slot = int_value;
  }
  if (mini_gnb_c_extract_transport_int(text, "scheduled_prb_start", &int_value) == 0 && int_value >= 0) {
    request.prb_start = (uint16_t)int_value;
  }
  if (mini_gnb_c_extract_transport_int(text, "scheduled_prb_len", &int_value) == 0 && int_value > 0) {
    request.prb_len = (uint16_t)int_value;
  }
  if (mini_gnb_c_extract_transport_int(text, "mcs", &int_value) == 0 && int_value >= 0) {
    request.mcs = (uint8_t)int_value;
  }

  if (snprintf(default_payload,
               sizeof(default_payload),
               "SCRIPTED_PDCCH_DL|abs_slot=%d",
               request.abs_slot) < (int)sizeof(default_payload)) {
    mini_gnb_c_fill_payload_from_transport(text, default_payload, &request.payload);
  }

  mini_gnb_c_initial_access_scheduler_queue_dl_data(&simulator->scheduler, &request, &simulator->metrics);
  mini_gnb_c_mark_scripted_dl_plan(simulator, &request);
  mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                 "scripted_pdcch",
                                 "Queued DL schedule from scripted PDCCH.",
                                 slot->abs_slot,
                                 "rnti=%u,dl_abs_slot=%d,prb_start=%u,prb_len=%u,mcs=%u,dci=%s,path=%s",
                                 request.c_rnti,
                                 request.abs_slot,
                                 request.prb_start,
                                 request.prb_len,
                                 request.mcs,
                                 mini_gnb_c_dci_format_to_string(request.dci_format),
                                 path);
  free(text);
}

static void mini_gnb_c_process_scripted_pdcch_ul(mini_gnb_c_simulator_t* simulator,
                                                 const mini_gnb_c_slot_indication_t* slot) {
  char path[MINI_GNB_C_MAX_PATH];
  char dci_value[16];
  char* text = NULL;
  mini_gnb_c_ul_data_schedule_request_t request;
  int int_value = 0;
  uint16_t rnti = 0U;

  if (simulator == NULL || slot == NULL || simulator->config.sim.scripted_pdcch_dir[0] == '\0' ||
      mini_gnb_c_build_script_path(simulator->config.sim.scripted_pdcch_dir,
                                   slot->abs_slot,
                                   "SCRIPT_PDCCH_UL",
                                   path,
                                   sizeof(path)) != 0) {
    return;
  }

  text = mini_gnb_c_read_text_file(path);
  if (text == NULL) {
    return;
  }

  if (mini_gnb_c_extract_transport_value(text, "dci_format", dci_value, sizeof(dci_value)) == 0 &&
      strcmp(dci_value, "DCI0_1") != 0 && strcmp(dci_value, "DCI0_0") != 0) {
    free(text);
    return;
  }

  memset(&request, 0, sizeof(request));
  rnti = mini_gnb_c_resolve_script_rnti(simulator, text);
  if (rnti == 0U) {
    free(text);
    return;
  }

  request.c_rnti = rnti;
  request.pdcch_abs_slot = slot->abs_slot;
  request.k2 = 2U;
  request.purpose = mini_gnb_c_parse_ul_purpose(text, MINI_GNB_C_UL_DATA_PURPOSE_PAYLOAD);
  request.prb_start = request.purpose == MINI_GNB_C_UL_DATA_PURPOSE_BSR ? 60U : 48U;
  request.prb_len = request.purpose == MINI_GNB_C_UL_DATA_PURPOSE_BSR ? 8U : 24U;
  request.mcs = request.purpose == MINI_GNB_C_UL_DATA_PURPOSE_BSR ? 4U : 8U;
  if (mini_gnb_c_extract_transport_int(text, "k2", &int_value) == 0 && int_value >= 0) {
    request.k2 = (uint8_t)int_value;
  }
  request.abs_slot = request.pdcch_abs_slot + request.k2;
  if (mini_gnb_c_extract_transport_int(text, "scheduled_abs_slot", &int_value) == 0) {
    request.abs_slot = int_value;
  }
  if (mini_gnb_c_extract_transport_int(text, "scheduled_prb_start", &int_value) == 0 && int_value >= 0) {
    request.prb_start = (uint16_t)int_value;
  }
  if (mini_gnb_c_extract_transport_int(text, "scheduled_prb_len", &int_value) == 0 && int_value > 0) {
    request.prb_len = (uint16_t)int_value;
  }
  if (mini_gnb_c_extract_transport_int(text, "mcs", &int_value) == 0 && int_value >= 0) {
    request.mcs = (uint8_t)int_value;
  }

  mini_gnb_c_initial_access_scheduler_queue_ul_data_grant(&simulator->scheduler, &request, &simulator->metrics);
  mini_gnb_c_arm_scripted_ul_data(simulator, &request);
  mini_gnb_c_mark_scripted_ul_plan(simulator, &request);
  mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                 "scripted_pdcch",
                                 "Queued UL expectation from scripted PDCCH.",
                                 slot->abs_slot,
                                 "rnti=%u,purpose=%s,ul_abs_slot=%d,prb_start=%u,prb_len=%u,mcs=%u,k2=%u,path=%s",
                                 request.c_rnti,
                                 mini_gnb_c_ul_data_purpose_string(request.purpose),
                                 request.abs_slot,
                                 request.prb_start,
                                 request.prb_len,
                                 request.mcs,
                                 request.k2,
                                 path);
  free(text);
}

static void mini_gnb_c_process_scripted_slot_controls(mini_gnb_c_simulator_t* simulator,
                                                      const mini_gnb_c_slot_indication_t* slot) {
  if (simulator == NULL || slot == NULL) {
    return;
  }

  mini_gnb_c_process_scripted_dl_schedule(simulator, slot);
  mini_gnb_c_process_scripted_ul_schedule(simulator, slot);
  mini_gnb_c_process_scripted_pdcch_dl(simulator, slot);
  mini_gnb_c_process_scripted_pdcch_ul(simulator, slot);
}

static void mini_gnb_c_queue_connected_ul_grant(mini_gnb_c_simulator_t* simulator,
                                                mini_gnb_c_ue_context_t* ue_context,
                                                int trigger_abs_slot,
                                                mini_gnb_c_ul_data_purpose_t purpose,
                                                uint16_t prb_start,
                                                uint16_t prb_len,
                                                uint8_t mcs) {
  mini_gnb_c_ul_data_schedule_request_t request;
  mini_gnb_c_ul_data_grant_t armed_ul_grant;

  if (simulator == NULL || ue_context == NULL) {
    return;
  }

  memset(&request, 0, sizeof(request));
  request.c_rnti = ue_context->c_rnti;
  request.pdcch_abs_slot = trigger_abs_slot + 1;
  request.k2 = (uint8_t)simulator->config.sim.post_msg4_ul_data_k2;
  request.abs_slot = request.pdcch_abs_slot + simulator->config.sim.post_msg4_ul_data_k2;
  request.prb_start = prb_start;
  request.prb_len = prb_len;
  request.mcs = mcs;
  request.purpose = purpose;
  mini_gnb_c_initial_access_scheduler_queue_ul_data_grant(&simulator->scheduler, &request, &simulator->metrics);

  memset(&armed_ul_grant, 0, sizeof(armed_ul_grant));
  armed_ul_grant.c_rnti = request.c_rnti;
  armed_ul_grant.abs_slot = request.abs_slot;
  armed_ul_grant.purpose = request.purpose;
  mini_gnb_c_mock_radio_frontend_arm_ul_data(&simulator->radio, &armed_ul_grant);

  if (purpose == MINI_GNB_C_UL_DATA_PURPOSE_BSR) {
    ue_context->small_ul_grant_abs_slot = request.abs_slot;
  } else {
    ue_context->large_ul_grant_abs_slot = request.abs_slot;
  }
}

static void mini_gnb_c_schedule_post_msg4_traffic(mini_gnb_c_simulator_t* simulator,
                                                  mini_gnb_c_ue_context_t* ue_context,
                                                  int msg4_abs_slot) {
  mini_gnb_c_dl_data_schedule_request_t dl_request;
  char payload_text[MINI_GNB_C_MAX_TEXT];
  int sr_abs_slot = 0;

  if (simulator == NULL || ue_context == NULL || ue_context->traffic_plan_scheduled ||
      !simulator->config.sim.post_msg4_traffic_enabled || simulator->config.sim.scripted_schedule_dir[0] != '\0' ||
      simulator->config.sim.scripted_pdcch_dir[0] != '\0') {
    return;
  }

  memset(&dl_request, 0, sizeof(dl_request));
  dl_request.c_rnti = ue_context->c_rnti;
  dl_request.abs_slot = msg4_abs_slot + simulator->config.sim.post_msg4_dl_data_delay_slots;
  dl_request.prb_start = 52U;
  dl_request.prb_len = 24U;
  dl_request.mcs = 9U;
  dl_request.dci_format = MINI_GNB_C_DCI_FORMAT_1_1;
  sr_abs_slot = dl_request.abs_slot + simulator->config.sim.post_msg4_ul_grant_delay_slots;
  if (snprintf(payload_text,
               sizeof(payload_text),
               "PUCCH_CFG|sr_abs_slot=%d",
               sr_abs_slot) < (int)sizeof(payload_text)) {
    (void)mini_gnb_c_buffer_set_text(&dl_request.payload, payload_text);
  }
  mini_gnb_c_initial_access_scheduler_queue_dl_data(&simulator->scheduler, &dl_request, &simulator->metrics);
  mini_gnb_c_mock_radio_frontend_arm_pucch_sr(&simulator->radio, ue_context->c_rnti, sr_abs_slot);

  ue_context->traffic_plan_scheduled = true;
  ue_context->dl_data_abs_slot = dl_request.abs_slot;
  mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                 "connected_scheduler",
                                 "Queued post-Msg4 PUCCH config and SR occasion.",
                                 msg4_abs_slot,
                                 "c_rnti=%u,dl_cfg_abs_slot=%d,sr_abs_slot=%d",
                                 ue_context->c_rnti,
                                 ue_context->dl_data_abs_slot,
                                 sr_abs_slot);
}

void mini_gnb_c_simulator_init(mini_gnb_c_simulator_t* simulator,
                               const mini_gnb_c_config_t* config,
                               const char* output_dir) {
  if (simulator == NULL || config == NULL) {
    return;
  }

  memset(simulator, 0, sizeof(*simulator));
  simulator->config = *config;
  mini_gnb_c_resolve_optional_dir_in_place(simulator->config.sim.local_exchange_dir,
                                           sizeof(simulator->config.sim.local_exchange_dir));
  mini_gnb_c_resolve_optional_dir_in_place(simulator->config.sim.scripted_schedule_dir,
                                           sizeof(simulator->config.sim.scripted_schedule_dir));
  mini_gnb_c_resolve_optional_dir_in_place(simulator->config.sim.scripted_pdcch_dir,
                                           sizeof(simulator->config.sim.scripted_pdcch_dir));
  mini_gnb_c_metrics_trace_init(&simulator->metrics, output_dir);
  mini_gnb_c_slot_engine_init(&simulator->slot_engine, &simulator->config);
  mini_gnb_c_mock_radio_frontend_init(&simulator->radio, &simulator->config.rf, &simulator->config.sim);
  mini_gnb_c_broadcast_engine_init(&simulator->broadcast,
                                   &simulator->config.cell,
                                   &simulator->config.prach,
                                   &simulator->config.broadcast);
  mini_gnb_c_mock_prach_detector_init(&simulator->prach_detector, &simulator->config.sim);
  mini_gnb_c_ra_manager_init(&simulator->ra_manager, &simulator->config.prach, &simulator->config.sim);
  mini_gnb_c_initial_access_scheduler_init(&simulator->scheduler);
  mini_gnb_c_mock_msg3_receiver_init(&simulator->msg3_receiver, &simulator->config.sim);
  mini_gnb_c_mock_dl_phy_mapper_init(&simulator->dl_mapper);
  mini_gnb_c_ue_context_store_init(&simulator->ue_store);
  mini_gnb_c_gnb_core_bridge_init(&simulator->core_bridge,
                                  &simulator->config.core,
                                  simulator->config.sim.local_exchange_dir);
}

int mini_gnb_c_simulator_run(mini_gnb_c_simulator_t* simulator,
                             mini_gnb_c_run_summary_t* out_summary) {
  int abs_slot = 0;

  if (simulator == NULL || out_summary == NULL) {
    return -1;
  }

  mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                 "main",
                                 "Starting mini gNB Msg1-Msg4 prototype run.",
                                 -1,
                                 "total_slots=%d,slots_per_frame=%d",
                                 simulator->config.sim.total_slots,
                                 simulator->config.sim.slots_per_frame);

  for (abs_slot = 0; abs_slot < simulator->config.sim.total_slots; ++abs_slot) {
    mini_gnb_c_slot_indication_t slot;
    mini_gnb_c_radio_burst_t burst;
    mini_gnb_c_prach_indication_t prach;
    mini_gnb_c_ul_grant_for_msg3_t msg3_grants[MINI_GNB_C_MAX_MSG3_GRANTS];
    mini_gnb_c_ul_data_grant_t ul_data_pdcch_grants[MINI_GNB_C_MAX_UL_DATA_GRANTS];
    mini_gnb_c_ul_data_grant_t ul_data_rx_grants[MINI_GNB_C_MAX_UL_DATA_GRANTS];
    mini_gnb_c_dl_grant_t dl_grants[MINI_GNB_C_MAX_GRANTS + 2U];
    mini_gnb_c_tx_grid_patch_t patches[MINI_GNB_C_MAX_GRANTS + 2U];
    size_t msg3_grant_count = 0;
    size_t ul_data_pdcch_count = 0;
    size_t ul_data_rx_count = 0;
    size_t dl_grant_count = 0;
    size_t patch_count = 0;
    size_t i = 0;

    mini_gnb_c_slot_engine_make_slot(&simulator->slot_engine, abs_slot, &slot);
    mini_gnb_c_mock_radio_frontend_receive(&simulator->radio, &slot, &burst);
    if (burst.ul_type != MINI_GNB_C_UL_BURST_NONE) {
      mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                     "radio_rx",
                                     "Received UL burst.",
                                     slot.abs_slot,
                                     "type=%s,nof_samples=%u,rnti=%u,preamble_id=%u,tbsize=%u",
                                     mini_gnb_c_ul_burst_type_to_string(burst.ul_type),
                                     burst.nof_samples,
                                     burst.rnti,
                                     burst.preamble_id,
                                     burst.tbsize);
    }
    mini_gnb_c_ra_manager_expire(&simulator->ra_manager, &slot, &simulator->metrics);

    if (mini_gnb_c_mock_prach_detector_detect(&simulator->prach_detector, &slot, &burst, &prach) &&
        prach.valid) {
      mini_gnb_c_ra_schedule_request_t request;
      mini_gnb_c_metrics_trace_increment_named(&simulator->metrics, "prach_detect_ok", 1U);
      mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                     "prach_detector",
                                     "Detected PRACH occasion.",
                                     slot.abs_slot,
                                     "preamble_id=%u,ta_est=%d,peak_metric=%.2f",
                                     prach.preamble_id,
                                     prach.ta_est,
                                     prach.peak_metric);
      if (mini_gnb_c_ra_manager_on_prach(&simulator->ra_manager,
                                         &prach,
                                         &slot,
                                         &simulator->metrics,
                                         &request)) {
        mini_gnb_c_initial_access_scheduler_queue_rar(&simulator->scheduler,
                                                      &request,
                                                      &simulator->metrics);
        mini_gnb_c_mock_radio_frontend_arm_msg3(&simulator->radio, &request.ul_grant);
      }
    }

    msg3_grant_count = mini_gnb_c_initial_access_scheduler_pop_due_msg3_grants(&simulator->scheduler,
                                                                                slot.abs_slot,
                                                                                msg3_grants,
                                                                                MINI_GNB_C_MAX_MSG3_GRANTS);
    for (i = 0; i < msg3_grant_count; ++i) {
      mini_gnb_c_msg3_decode_indication_t msg3;
      char mac_pdu_hex[MINI_GNB_C_MAX_PAYLOAD * 2U + 1U];

      if (!mini_gnb_c_mock_msg3_receiver_decode(&simulator->msg3_receiver,
                                                &slot,
                                                &msg3_grants[i],
                                                &burst,
                                                &msg3)) {
        const bool has_due_msg3_burst = slot.abs_slot == msg3_grants[i].abs_slot &&
                                        burst.ul_type == MINI_GNB_C_UL_BURST_MSG3 &&
                                        burst.nof_samples > 0U;

        mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                       "pusch_msg3_receiver",
                                       has_due_msg3_burst && burst.rnti != 0U &&
                                               burst.rnti != msg3_grants[i].tc_rnti
                                           ? "Rejected Msg3 burst due to TC-RNTI mismatch."
                                           : "No Msg3 burst decoded for due UL grant.",
                                       slot.abs_slot,
                                       "rnti=%u,expected_abs_slot=%d,ul_burst=%s,burst_rnti=%u",
                                       msg3_grants[i].tc_rnti,
                                       msg3_grants[i].abs_slot,
                                       mini_gnb_c_ul_burst_type_to_string(burst.ul_type),
                                       burst.rnti);
        continue;
      }

      mini_gnb_c_metrics_trace_increment_named(&simulator->metrics,
                                               msg3.crc_ok ? "msg3_crc_ok" : "msg3_crc_fail",
                                               1U);
      (void)mini_gnb_c_bytes_to_hex(msg3.mac_pdu.bytes,
                                    msg3.mac_pdu.len,
                                    mac_pdu_hex,
                                    sizeof(mac_pdu_hex));
      mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                     "pusch_msg3_receiver",
                                     "Decoded Msg3 candidate.",
                                     slot.abs_slot,
                                     "rnti=%u,crc_ok=%s,snr_db=%.2f,evm=%.2f,mac_pdu=%s",
                                     msg3.rnti,
                                     mini_gnb_c_bool_string(msg3.crc_ok),
                                     msg3.snr_db,
                                     msg3.evm,
                                     mac_pdu_hex);

      if (!msg3.crc_ok) {
        continue;
      }

      {
        mini_gnb_c_mac_ul_parse_result_t mac_result;
        char lcid_sequence[MINI_GNB_C_MAX_TEXT];

        mini_gnb_c_mac_ul_demux_parse(&msg3.mac_pdu, &mac_result);
        mini_gnb_c_join_lcid_sequence(&mac_result, lcid_sequence, sizeof(lcid_sequence));
        mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                       "mac_ul_demux",
                                       "Parsed Msg3 MAC PDU.",
                                       slot.abs_slot,
                                       "parse_ok=%s,has_ul_ccch=%s,has_crnti_ce=%s,lcid_sequence=%s",
                                       mini_gnb_c_bool_string(mac_result.parse_ok),
                                       mini_gnb_c_bool_string(mac_result.has_ul_ccch),
                                       mini_gnb_c_bool_string(mac_result.has_crnti_ce),
                                       lcid_sequence);

        if (!mac_result.parse_ok || !mac_result.has_ul_ccch) {
          continue;
        }

        {
          mini_gnb_c_rrc_setup_request_info_t request_info;
          mini_gnb_c_rrc_setup_blob_t rrc_setup;
          char contention_id_hex[MINI_GNB_C_MAX_TEXT];

          mini_gnb_c_parse_rrc_setup_request(&mac_result.ul_ccch_sdu, &request_info);
          (void)mini_gnb_c_bytes_to_hex(request_info.contention_id48,
                                        6U,
                                        contention_id_hex,
                                        sizeof(contention_id_hex));
          mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                         "rrc_ccch_stub",
                                         "Parsed RRCSetupRequest.",
                                         slot.abs_slot,
                                         "valid=%s,establishment_cause=%u,ue_identity_type=%u,contention_id48=%s",
                                         mini_gnb_c_bool_string(request_info.valid),
                                         request_info.establishment_cause,
                                         request_info.ue_identity_type,
                                         contention_id_hex);

          if (!request_info.valid) {
            continue;
          }

          mini_gnb_c_build_rrc_setup(&request_info, &rrc_setup);
          if (simulator->ra_manager.has_active_context) {
            mini_gnb_c_ue_context_t* ue_context =
                mini_gnb_c_ue_context_store_promote(&simulator->ue_store,
                                                    &simulator->ra_manager.active_context,
                                                    &request_info,
                                                    slot.abs_slot);
            if (ue_context != NULL) {
              if (mini_gnb_c_gnb_core_bridge_on_ue_promoted(&simulator->core_bridge,
                                                            ue_context,
                                                            &simulator->metrics,
                                                            slot.abs_slot) != 0) {
                mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                               "gnb_core_bridge",
                                               "Failed to prepare InitialUEMessage for promoted UE.",
                                               slot.abs_slot,
                                               "c_rnti=%u",
                                               ue_context->c_rnti);
              }
              mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                             "ue_context_store",
                                             "Promoted RA context into minimal UE context.",
                                             slot.abs_slot,
                                             "tc_rnti=%u,c_rnti=%u,contention_id48=%s",
                                             ue_context->tc_rnti,
                                             ue_context->c_rnti,
                                             contention_id_hex);
            }
          }

          {
            mini_gnb_c_msg4_schedule_request_t msg4_request;
            if (mini_gnb_c_ra_manager_on_msg3_success(&simulator->ra_manager,
                                                      &msg3,
                                                      &mac_result,
                                                      &request_info,
                                                      &rrc_setup,
                                                      &slot,
                                                      &simulator->metrics,
                                                      &msg4_request)) {
              mini_gnb_c_initial_access_scheduler_queue_msg4(&simulator->scheduler,
                                                             &msg4_request,
                                                             &simulator->metrics);
            }
          }
        }
      }
    }

    if (burst.ul_type == MINI_GNB_C_UL_BURST_PUCCH_SR && burst.nof_samples > 0U) {
      mini_gnb_c_ue_context_t* ue_context = mini_gnb_c_find_expected_connected_ue(simulator, burst.rnti);
      const bool rnti_match = ue_context != NULL && (burst.rnti == 0U || burst.rnti == ue_context->c_rnti);
      const bool sr_ok = !burst.crc_ok_override_valid || burst.crc_ok_override;

      if (ue_context == NULL || !rnti_match || !sr_ok) {
        mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                       "pucch_sr_detector",
                                       (ue_context != NULL && !rnti_match)
                                           ? "Rejected PUCCH SR due to C-RNTI mismatch."
                                           : "Ignored invalid PUCCH SR candidate.",
                                       slot.abs_slot,
                                       "burst_rnti=%u,ue_c_rnti=%u,crc_ok=%s",
                                       burst.rnti,
                                       ue_context != NULL ? ue_context->c_rnti : 0U,
                                       mini_gnb_c_bool_string(sr_ok));
      } else if (!ue_context->pucch_sr_detected) {
        mini_gnb_c_metrics_trace_increment_named(&simulator->metrics, "pucch_sr_detect_ok", 1U);
        ue_context->pucch_sr_detected = true;
        ue_context->pucch_sr_abs_slot = slot.abs_slot;
        mini_gnb_c_queue_connected_ul_grant(simulator,
                                            ue_context,
                                            slot.abs_slot,
                                            MINI_GNB_C_UL_DATA_PURPOSE_BSR,
                                            60U,
                                            8U,
                                            4U);
        mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                       "pucch_sr_detector",
                                       "Detected PUCCH SR and queued compact UL BSR grant.",
                                       slot.abs_slot,
                                       "c_rnti=%u,small_ul_grant_abs_slot=%d",
                                       ue_context->c_rnti,
                                       ue_context->small_ul_grant_abs_slot);
      }
    }

    ul_data_rx_count = mini_gnb_c_initial_access_scheduler_pop_due_ul_data_rx(&simulator->scheduler,
                                                                               slot.abs_slot,
                                                                               ul_data_rx_grants,
                                                                               MINI_GNB_C_MAX_UL_DATA_GRANTS);
    for (i = 0; i < ul_data_rx_count; ++i) {
      const char* receiver_module = ul_data_rx_grants[i].purpose == MINI_GNB_C_UL_DATA_PURPOSE_BSR
                                        ? "pusch_bsr_receiver"
                                        : "pusch_data_receiver";
      const bool has_due_ul_data_burst = slot.abs_slot == ul_data_rx_grants[i].abs_slot &&
                                         burst.ul_type == MINI_GNB_C_UL_BURST_DATA && burst.nof_samples > 0U;
      const bool rnti_match = burst.rnti == 0U || burst.rnti == ul_data_rx_grants[i].c_rnti;
      const bool crc_ok = burst.crc_ok_override_valid ? burst.crc_ok_override : simulator->config.sim.ul_data_crc_ok;
      char payload_hex[MINI_GNB_C_MAX_PAYLOAD * 2U + 1U];

      if (!has_due_ul_data_burst || !rnti_match) {
        mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                       receiver_module,
                                       has_due_ul_data_burst && !rnti_match
                                           ? "Rejected scheduled UL burst due to C-RNTI mismatch."
                                           : "No scheduled UL burst decoded for due UL grant.",
                                       slot.abs_slot,
                                       "purpose=%s,rnti=%u,expected_abs_slot=%d,ul_burst=%s,burst_rnti=%u",
                                       mini_gnb_c_ul_data_purpose_string(ul_data_rx_grants[i].purpose),
                                       ul_data_rx_grants[i].c_rnti,
                                       ul_data_rx_grants[i].abs_slot,
                                       mini_gnb_c_ul_burst_type_to_string(burst.ul_type),
                                       burst.rnti);
        continue;
      }

      (void)mini_gnb_c_bytes_to_hex(burst.mac_pdu.bytes, burst.mac_pdu.len, payload_hex, sizeof(payload_hex));
      if (ul_data_rx_grants[i].purpose == MINI_GNB_C_UL_DATA_PURPOSE_BSR) {
        const int buffer_size_bytes = crc_ok ? mini_gnb_c_parse_bsr_buffer_size_bytes(&burst.mac_pdu) : -1;
        const uint16_t tbsize =
            burst.tbsize != 0U ? burst.tbsize
                               : mini_gnb_c_lookup_tbsize(ul_data_rx_grants[i].prb_len, ul_data_rx_grants[i].mcs);
        mini_gnb_c_metrics_trace_increment_named(&simulator->metrics,
                                                 (crc_ok && buffer_size_bytes >= 0) ? "ul_bsr_rx_ok"
                                                                                    : "ul_bsr_crc_fail",
                                                 1U);
        mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                       receiver_module,
                                       (crc_ok && buffer_size_bytes >= 0)
                                           ? "Decoded scheduled UL BSR candidate."
                                           : "Failed to decode scheduled UL BSR candidate.",
                                       slot.abs_slot,
                                       "rnti=%u,crc_ok=%s,snr_db=%.2f,evm=%.2f,tbsize=%u,buffer_size_bytes=%d,payload=%s",
                                       ul_data_rx_grants[i].c_rnti,
                                       mini_gnb_c_bool_string(crc_ok),
                                       burst.snr_db,
                                       burst.evm,
                                       tbsize,
                                       buffer_size_bytes,
                                       payload_hex);

        if (crc_ok && buffer_size_bytes >= 0) {
          mini_gnb_c_ue_context_t* ue_context =
              mini_gnb_c_find_ue_context(&simulator->ue_store, ul_data_rx_grants[i].c_rnti);
          if (ue_context != NULL) {
            ue_context->ul_bsr_received = true;
            ue_context->ul_bsr_abs_slot = slot.abs_slot;
            ue_context->ul_bsr_buffer_size_bytes = buffer_size_bytes;
            if (simulator->config.sim.scripted_schedule_dir[0] == '\0' &&
                simulator->config.sim.scripted_pdcch_dir[0] == '\0') {
              mini_gnb_c_queue_connected_ul_grant(simulator,
                                                  ue_context,
                                                  slot.abs_slot,
                                                  MINI_GNB_C_UL_DATA_PURPOSE_PAYLOAD,
                                                  48U,
                                                  mini_gnb_c_select_large_ul_prb_len(buffer_size_bytes),
                                                  8U);
              mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                             "connected_scheduler",
                                             "Queued large UL grant after BSR.",
                                             slot.abs_slot,
                                             "c_rnti=%u,buffer_size_bytes=%d,large_ul_grant_abs_slot=%d",
                                             ue_context->c_rnti,
                                             buffer_size_bytes,
                                             ue_context->large_ul_grant_abs_slot);
            }
          }
        }
      } else {
        const uint16_t tbsize =
            burst.tbsize != 0U ? burst.tbsize
                               : mini_gnb_c_lookup_tbsize(ul_data_rx_grants[i].prb_len, ul_data_rx_grants[i].mcs);
        mini_gnb_c_metrics_trace_increment_named(&simulator->metrics,
                                                 crc_ok ? "ul_data_rx_ok" : "ul_data_crc_fail",
                                                 1U);
        mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                       receiver_module,
                                       "Decoded scheduled UL data candidate.",
                                       slot.abs_slot,
                                       "rnti=%u,crc_ok=%s,snr_db=%.2f,evm=%.2f,tbsize=%u,payload=%s",
                                       ul_data_rx_grants[i].c_rnti,
                                       mini_gnb_c_bool_string(crc_ok),
                                       burst.snr_db,
                                       burst.evm,
                                       tbsize,
                                       payload_hex);

        if (crc_ok) {
          mini_gnb_c_ue_context_t* ue_context =
              mini_gnb_c_find_ue_context(&simulator->ue_store, ul_data_rx_grants[i].c_rnti);
          if (ue_context != NULL) {
            ue_context->ul_data_received = true;
            ue_context->ul_data_abs_slot = slot.abs_slot;
          }
        }
      }
    }

    mini_gnb_c_process_scripted_slot_controls(simulator, &slot);

    ul_data_pdcch_count = mini_gnb_c_initial_access_scheduler_pop_due_ul_data_pdcch(&simulator->scheduler,
                                                                                     slot.abs_slot,
                                                                                     ul_data_pdcch_grants,
                                                                                     MINI_GNB_C_MAX_UL_DATA_GRANTS);
    for (i = 0; i < ul_data_pdcch_count; ++i) {
      mini_gnb_c_mock_radio_frontend_submit_pdcch(&simulator->radio,
                                                  &slot,
                                                  &ul_data_pdcch_grants[i].pdcch,
                                                  &simulator->metrics);
    }

    dl_grant_count = mini_gnb_c_initial_access_scheduler_pop_due_downlink(&simulator->scheduler,
                                                                           slot.abs_slot,
                                                                           dl_grants,
                                                                           MINI_GNB_C_MAX_GRANTS);
    dl_grant_count += mini_gnb_c_broadcast_schedule(&simulator->broadcast,
                                                    &slot,
                                                    &dl_grants[dl_grant_count],
                                                    (MINI_GNB_C_MAX_GRANTS + 2U) - dl_grant_count);

    if (dl_grant_count > 0U) {
      patch_count = mini_gnb_c_mock_dl_phy_mapper_map(&simulator->dl_mapper,
                                                      &slot,
                                                      dl_grants,
                                                      dl_grant_count,
                                                      patches,
                                                      MINI_GNB_C_MAX_GRANTS + 2U);
      mini_gnb_c_mock_radio_frontend_submit_tx(&simulator->radio,
                                               &slot,
                                               patches,
                                               patch_count,
                                               &simulator->metrics);

      for (i = 0; i < dl_grant_count; ++i) {
        if (dl_grants[i].type == MINI_GNB_C_DL_OBJ_RAR) {
          mini_gnb_c_metrics_trace_increment_named(&simulator->metrics, "rar_sent", 1U);
          mini_gnb_c_ra_manager_mark_rar_sent(&simulator->ra_manager,
                                              dl_grants[i].rnti,
                                              &slot,
                                              &simulator->metrics);
        } else if (dl_grants[i].type == MINI_GNB_C_DL_OBJ_MSG4) {
          mini_gnb_c_metrics_trace_increment_named(&simulator->metrics, "rrcsetup_sent", 1U);
          mini_gnb_c_ra_manager_mark_msg4_sent(&simulator->ra_manager,
                                               dl_grants[i].rnti,
                                               &slot,
                                               &simulator->metrics);
          mini_gnb_c_ue_context_store_mark_rrc_setup_sent(&simulator->ue_store,
                                                          dl_grants[i].rnti,
                                                          slot.abs_slot);
          mini_gnb_c_schedule_post_msg4_traffic(simulator,
                                                mini_gnb_c_find_ue_context(&simulator->ue_store, dl_grants[i].rnti),
                                                slot.abs_slot);
        } else if (dl_grants[i].type == MINI_GNB_C_DL_OBJ_DATA) {
          mini_gnb_c_ue_context_t* ue_context =
              mini_gnb_c_find_ue_context(&simulator->ue_store, dl_grants[i].rnti);
          mini_gnb_c_metrics_trace_increment_named(&simulator->metrics, "dl_data_sent", 1U);
          if (ue_context != NULL) {
            ue_context->dl_data_sent = true;
            ue_context->dl_data_abs_slot = slot.abs_slot;
          }
        }
      }
    }

    if (simulator->ue_store.count > 0U &&
        mini_gnb_c_gnb_core_bridge_poll_ue_nas(&simulator->core_bridge,
                                               simulator->ue_store.contexts,
                                               simulator->ue_store.count,
                                               &simulator->metrics,
                                               slot.abs_slot) != 0) {
      mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                     "gnb_core_bridge",
                                     "Failed to relay follow-up UE UL_NAS event.",
                                     slot.abs_slot,
                                     "ue_count=%u,next_ul_nas_sequence=%u",
                                     (unsigned)simulator->ue_store.count,
                                     simulator->core_bridge.next_ue_to_gnb_nas_sequence);
    }

    {
      mini_gnb_c_slot_perf_t perf;
      memset(&perf, 0, sizeof(perf));
      perf.abs_slot = slot.abs_slot;
      perf.mac_latency_us = 120 + (int)slot.slot;
      perf.dl_build_latency_us = 80 + (int)((dl_grant_count + ul_data_pdcch_count) * 5U);
      perf.ul_decode_latency_us = 60 + (int)((msg3_grant_count + ul_data_rx_count) * 10U);
      mini_gnb_c_metrics_trace_add_slot_perf(&simulator->metrics, &perf);
    }
  }

  return mini_gnb_c_metrics_trace_flush(&simulator->metrics,
                                        simulator->ra_manager.has_active_context,
                                        simulator->ra_manager.has_active_context
                                            ? &simulator->ra_manager.active_context
                                            : NULL,
                                        simulator->ue_store.contexts,
                                        simulator->ue_store.count,
                                        simulator->radio.tx_burst_count,
                                        simulator->radio.last_hw_time_ns,
                                        out_summary);
}
