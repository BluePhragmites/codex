#include "mini_gnb_c/radio/mock_radio_frontend.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "mini_gnb_c/common/hex.h"
#include "mini_gnb_c/common/json_utils.h"
#include "mini_gnb_c/link/json_link.h"
#include "mini_gnb_c/metrics/metrics_trace.h"

static int mini_gnb_c_ensure_directory_recursive(const char* path) {
  char temp[MINI_GNB_C_MAX_PATH];
  size_t i = 0;

  if (path == NULL) {
    return -1;
  }

  (void)snprintf(temp, sizeof(temp), "%s", path);
  for (i = 1; temp[i] != '\0'; ++i) {
    if (temp[i] == '/' || temp[i] == '\\') {
      char saved = temp[i];
      temp[i] = '\0';
      if (strlen(temp) > 0U && mkdir(temp, 0777) != 0 && errno != EEXIST) {
        temp[i] = saved;
        return -1;
      }
      temp[i] = saved;
    }
  }

  if (mkdir(temp, 0777) != 0 && errno != EEXIST) {
    return -1;
  }
  return 0;
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

static bool mini_gnb_c_directory_exists(const char* path) {
  struct stat st;

  if (path == NULL || path[0] == '\0') {
    return false;
  }
  if (stat(path, &st) != 0) {
    return false;
  }
  return S_ISDIR(st.st_mode) != 0;
}

static int mini_gnb_c_resolve_input_dir(const char* configured_path, char* out, size_t out_size) {
  if (configured_path == NULL || configured_path[0] == '\0' || out == NULL || out_size == 0U) {
    return -1;
  }

  if (mini_gnb_c_path_is_absolute(configured_path)) {
    return snprintf(out, out_size, "%s", configured_path) < (int)out_size ? 0 : -1;
  }

  return mini_gnb_c_join_path(MINI_GNB_C_SOURCE_DIR, configured_path, out, out_size);
}

static int mini_gnb_c_build_slot_object_path(const char* dir,
                                             int abs_slot,
                                             const char* direction,
                                             const char* object_name,
                                             const char* extension,
                                             char* out,
                                             size_t out_size) {
  if (dir == NULL || dir[0] == '\0' || direction == NULL || object_name == NULL || extension == NULL ||
      out == NULL || out_size == 0U) {
    return -1;
  }

  return snprintf(out,
                  out_size,
                  "%s/slot_%d_%s_OBJ_%s.%s",
                  dir,
                  abs_slot,
                  direction,
                  object_name,
                  extension) < (int)out_size
             ? 0
             : -1;
}

static int mini_gnb_c_build_local_exchange_channel_dir(const char* root_dir,
                                                       const char* channel,
                                                       char* out,
                                                       size_t out_size) {
  if (root_dir == NULL || channel == NULL || out == NULL || out_size == 0U) {
    return -1;
  }

  return mini_gnb_c_join_path(root_dir, channel, out, out_size);
}

static int mini_gnb_c_find_local_exchange_event_path(const char* root_dir,
                                                     const char* channel,
                                                     const char* source,
                                                     const uint32_t sequence,
                                                     char* out,
                                                     size_t out_size) {
  DIR* dir = NULL;
  struct dirent* entry = NULL;
  char channel_dir[MINI_GNB_C_MAX_PATH];
  char prefix[64];

  if (root_dir == NULL || channel == NULL || source == NULL || out == NULL || out_size == 0U) {
    return -1;
  }
  if (mini_gnb_c_build_local_exchange_channel_dir(root_dir, channel, channel_dir, sizeof(channel_dir)) != 0) {
    return -1;
  }
  if (snprintf(prefix, sizeof(prefix), "seq_%06u_%s_", sequence, source) >= (int)sizeof(prefix)) {
    return -1;
  }

  dir = opendir(channel_dir);
  if (dir == NULL) {
    return -1;
  }

  while ((entry = readdir(dir)) != NULL) {
    size_t name_len = strlen(entry->d_name);

    if (strncmp(entry->d_name, prefix, strlen(prefix)) != 0 || name_len < 6U ||
        strcmp(entry->d_name + name_len - 5U, ".json") != 0) {
      continue;
    }
    if (mini_gnb_c_join_path(channel_dir, entry->d_name, out, out_size) != 0) {
      closedir(dir);
      return -1;
    }
    closedir(dir);
    return 0;
  }

  closedir(dir);
  return -1;
}

static const char* mini_gnb_c_dl_transport_name(mini_gnb_c_dl_object_type_t type) {
  switch (type) {
    case MINI_GNB_C_DL_OBJ_SSB:
      return "SSB";
    case MINI_GNB_C_DL_OBJ_SIB1:
      return "SIB1";
    case MINI_GNB_C_DL_OBJ_RAR:
      return "RAR";
    case MINI_GNB_C_DL_OBJ_MSG4:
      return "MSG4";
    case MINI_GNB_C_DL_OBJ_DATA:
      return "DATA";
    case MINI_GNB_C_DL_OBJ_PDCCH:
      return "PDCCH";
  }
  return "UNKNOWN";
}

static const char* mini_gnb_c_dci_direction_string(const mini_gnb_c_dci_format_t format) {
  switch (format) {
    case MINI_GNB_C_DCI_FORMAT_0_0:
    case MINI_GNB_C_DCI_FORMAT_0_1:
      return "UL";
    case MINI_GNB_C_DCI_FORMAT_1_0:
    case MINI_GNB_C_DCI_FORMAT_1_1:
      return "DL";
  }
  return "UNKNOWN";
}

static const char* mini_gnb_c_ul_data_purpose_name(const mini_gnb_c_ul_data_purpose_t purpose) {
  switch (purpose) {
    case MINI_GNB_C_UL_DATA_PURPOSE_BSR:
      return "BSR";
    case MINI_GNB_C_UL_DATA_PURPOSE_PAYLOAD:
      return "DATA";
  }
  return "DATA";
}

static const char* mini_gnb_c_scheduled_object_name(const mini_gnb_c_pdcch_dci_t* pdcch) {
  if (pdcch == NULL) {
    return "UNKNOWN";
  }
  if (pdcch->scheduled_ul_type != MINI_GNB_C_UL_BURST_NONE) {
    switch (pdcch->scheduled_ul_type) {
      case MINI_GNB_C_UL_BURST_PRACH:
        return "PRACH";
      case MINI_GNB_C_UL_BURST_MSG3:
        return "MSG3";
      case MINI_GNB_C_UL_BURST_PUCCH_SR:
        return "PUCCH_SR";
      case MINI_GNB_C_UL_BURST_DATA:
        return mini_gnb_c_ul_data_purpose_name(pdcch->scheduled_ul_purpose);
      case MINI_GNB_C_UL_BURST_NONE:
        break;
    }
  }

  switch (pdcch->scheduled_dl_type) {
    case MINI_GNB_C_DL_OBJ_SSB:
      return "SSB";
    case MINI_GNB_C_DL_OBJ_SIB1:
      return "SIB1";
    case MINI_GNB_C_DL_OBJ_RAR:
      return "RAR";
    case MINI_GNB_C_DL_OBJ_MSG4:
      return "MSG4";
    case MINI_GNB_C_DL_OBJ_DATA:
      return "DATA";
    case MINI_GNB_C_DL_OBJ_PDCCH:
      return "PDCCH";
  }
  return "UNKNOWN";
}

static int mini_gnb_c_build_pdcch_transport_path(const char* dir,
                                                 const mini_gnb_c_slot_indication_t* slot,
                                                 const mini_gnb_c_pdcch_dci_t* pdcch,
                                                 char* out,
                                                 size_t out_size) {
  if (dir == NULL || slot == NULL || pdcch == NULL || out == NULL || out_size == 0U) {
    return -1;
  }

  return snprintf(out,
                  out_size,
                  "%s/slot_%d_DL_OBJ_PDCCH_%s_rnti_%u_%s.txt",
                  dir,
                  slot->abs_slot,
                  mini_gnb_c_dci_format_to_string(pdcch->format),
                  pdcch->rnti,
                  mini_gnb_c_scheduled_object_name(pdcch)) < (int)out_size
             ? 0
             : -1;
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

static int mini_gnb_c_extract_transport_double(const char* text, const char* key, double* out) {
  char value_text[64];
  char* end_ptr = NULL;

  if (out == NULL || mini_gnb_c_extract_transport_value(text, key, value_text, sizeof(value_text)) != 0) {
    return -1;
  }

  *out = strtod(value_text, &end_ptr);
  end_ptr = mini_gnb_c_ltrim(end_ptr);
  return (end_ptr != value_text && end_ptr != NULL && *end_ptr == '\0') ? 0 : -1;
}

static int mini_gnb_c_extract_transport_bool(const char* text, const char* key, bool* out) {
  char value_text[16];

  if (out == NULL || mini_gnb_c_extract_transport_value(text, key, value_text, sizeof(value_text)) != 0) {
    return -1;
  }

  if (strcmp(value_text, "true") == 0 || strcmp(value_text, "1") == 0 || strcmp(value_text, "TRUE") == 0) {
    *out = true;
    return 0;
  }
  if (strcmp(value_text, "false") == 0 || strcmp(value_text, "0") == 0 || strcmp(value_text, "FALSE") == 0) {
    *out = false;
    return 0;
  }
  return -1;
}

static const char* mini_gnb_c_find_json_value_start(const char* text, const char* key) {
  char pattern[64];
  const char* key_position = NULL;
  const char* colon = NULL;

  if (text == NULL || key == NULL) {
    return NULL;
  }
  if (snprintf(pattern, sizeof(pattern), "\"%s\"", key) >= (int)sizeof(pattern)) {
    return NULL;
  }

  key_position = strstr(text, pattern);
  if (key_position == NULL) {
    return NULL;
  }
  colon = strchr(key_position + strlen(pattern), ':');
  if (colon == NULL) {
    return NULL;
  }
  ++colon;
  while (*colon == ' ' || *colon == '\t' || *colon == '\r' || *colon == '\n') {
    ++colon;
  }
  return colon;
}

static int mini_gnb_c_extract_json_int(const char* text, const char* key, int* out) {
  const char* value_start = NULL;
  char* end_ptr = NULL;

  if (out == NULL) {
    return -1;
  }
  value_start = mini_gnb_c_find_json_value_start(text, key);
  if (value_start == NULL) {
    return -1;
  }

  *out = (int)strtol(value_start, &end_ptr, 10);
  return end_ptr != value_start ? 0 : -1;
}

static int mini_gnb_c_extract_json_double(const char* text, const char* key, double* out) {
  const char* value_start = NULL;
  char* end_ptr = NULL;

  if (out == NULL) {
    return -1;
  }
  value_start = mini_gnb_c_find_json_value_start(text, key);
  if (value_start == NULL) {
    return -1;
  }

  *out = strtod(value_start, &end_ptr);
  return end_ptr != value_start ? 0 : -1;
}

static int mini_gnb_c_extract_json_string(const char* text, const char* key, char* out, size_t out_size) {
  const char* value_start = NULL;
  size_t index = 0U;

  if (out == NULL || out_size == 0U) {
    return -1;
  }
  value_start = mini_gnb_c_find_json_value_start(text, key);
  if (value_start == NULL || *value_start != '"') {
    return -1;
  }
  ++value_start;

  while (value_start[index] != '\0' && value_start[index] != '"' && index + 1U < out_size) {
    out[index] = value_start[index];
    ++index;
  }
  if (value_start[index] != '"' || index + 1U >= out_size) {
    return -1;
  }
  out[index] = '\0';
  return 0;
}

static int mini_gnb_c_write_cf32(const char* path, const mini_gnb_c_tx_grid_patch_t* patch) {
  FILE* file = NULL;
  size_t i = 0;

  if (path == NULL || patch == NULL) {
    return -1;
  }

  file = fopen(path, "wb");
  if (file == NULL) {
    return -1;
  }

  for (i = 0; i < patch->sample_count; ++i) {
    float pair[2];
    pair[0] = patch->samples[i].real;
    pair[1] = patch->samples[i].imag;
    if (fwrite(pair, sizeof(float), 2U, file) != 2U) {
      fclose(file);
      return -1;
    }
  }

  fclose(file);
  return 0;
}

static int mini_gnb_c_write_iq_metadata(const char* path,
                                        const char* cf32_path,
                                        const mini_gnb_c_slot_indication_t* slot,
                                        const mini_gnb_c_tx_grid_patch_t* patch,
                                        uint64_t tx_index) {
  FILE* file = NULL;

  if (path == NULL || cf32_path == NULL || slot == NULL || patch == NULL) {
    return -1;
  }

  file = fopen(path, "wb");
  if (file == NULL) {
    return -1;
  }

  fprintf(file,
          "{"
          "\"abs_slot\":%d,"
          "\"sfn\":%u,"
          "\"slot\":%u,"
          "\"type\":\"%s\","
          "\"rnti\":%u,"
          "\"prb_start\":%u,"
          "\"prb_len\":%u,"
          "\"payload_len\":%zu,"
          "\"fft_size\":%u,"
          "\"cp_length\":%u,"
          "\"sample_count\":%zu,"
          "\"tx_index\":%llu,"
          "\"cf32_path\":\"%s\""
          "}\n",
          slot->abs_slot,
          slot->sfn,
          slot->slot,
          mini_gnb_c_dl_object_type_to_string(patch->type),
          patch->rnti,
          patch->prb_start,
          patch->prb_len,
          patch->payload_len,
          patch->fft_size,
          patch->cp_length,
          patch->sample_count,
          (unsigned long long)tx_index,
          cf32_path);

  fclose(file);
  return 0;
}

static void mini_gnb_c_payload_to_text(const mini_gnb_c_buffer_t* payload, char* out, size_t out_size) {
  static const char hex_chars[] = "0123456789ABCDEF";
  size_t i = 0;
  size_t out_index = 0;

  if (out == NULL || out_size == 0U) {
    return;
  }

  out[0] = '\0';
  if (payload == NULL) {
    return;
  }

  for (i = 0; i < payload->len && out_index + 1U < out_size; ++i) {
    const unsigned char ch = payload->bytes[i];
    if (ch >= 32U && ch <= 126U && ch != '\\') {
      out[out_index++] = (char)ch;
    } else if (out_index + 4U < out_size) {
      out[out_index++] = '\\';
      out[out_index++] = 'x';
      out[out_index++] = hex_chars[(ch >> 4U) & 0xFU];
      out[out_index++] = hex_chars[ch & 0xFU];
    } else {
      break;
    }
  }
  out[out_index] = '\0';
}

static int mini_gnb_c_write_transport_text(const char* path,
                                           const char* cf32_path,
                                           const mini_gnb_c_slot_indication_t* slot,
                                           const mini_gnb_c_tx_grid_patch_t* patch,
                                           uint64_t tx_index) {
  FILE* file = NULL;
  char payload_hex[MINI_GNB_C_MAX_PAYLOAD * 2U + 1U];
  char payload_text[MINI_GNB_C_MAX_PAYLOAD * 4U + 1U];
  uint16_t tbsize = 0U;

  if (path == NULL || slot == NULL || patch == NULL) {
    return -1;
  }

  file = fopen(path, "wb");
  if (file == NULL) {
    return -1;
  }

  payload_hex[0] = '\0';
  (void)mini_gnb_c_bytes_to_hex(patch->payload.bytes, patch->payload.len, payload_hex, sizeof(payload_hex));
  mini_gnb_c_payload_to_text(&patch->payload, payload_text, sizeof(payload_text));
  if (patch->pdcch.valid) {
    tbsize = mini_gnb_c_lookup_tbsize(patch->prb_len, patch->pdcch.mcs);
  }

  fprintf(file,
          "direction=DL\n"
          "channel=%s\n"
          "abs_slot=%d\n"
          "sfn=%u\n"
          "slot=%u\n"
          "type=%s\n"
          "rnti=%u\n"
          "prb_start=%u\n"
          "prb_len=%u\n"
          "tbsize=%u\n"
          "payload_len=%zu\n"
          "payload_hex=%s\n"
          "payload_text=%s\n"
          "fft_size=%u\n"
          "cp_length=%u\n"
          "sample_count=%zu\n"
          "tx_index=%llu\n"
          "scheduled_by_pdcch=%s\n"
          "dci_format=%s\n"
          "cf32_path=%s\n",
          (patch->type == MINI_GNB_C_DL_OBJ_SSB)
              ? "PBCH"
              : (patch->type == MINI_GNB_C_DL_OBJ_PDCCH ? "PDCCH" : "PDSCH"),
          slot->abs_slot,
          slot->sfn,
          slot->slot,
          mini_gnb_c_dl_object_type_to_string(patch->type),
          patch->rnti,
          patch->prb_start,
          patch->prb_len,
          tbsize,
          patch->payload.len,
          payload_hex,
          payload_text,
          patch->fft_size,
          patch->cp_length,
          patch->sample_count,
          (unsigned long long)tx_index,
          patch->pdcch.valid ? "true" : "false",
          patch->pdcch.valid ? mini_gnb_c_dci_format_to_string(patch->pdcch.format) : "",
          (cf32_path != NULL) ? cf32_path : "");

  fclose(file);
  return 0;
}

static int mini_gnb_c_write_pdcch_transport_text(const char* path,
                                                 const mini_gnb_c_slot_indication_t* slot,
                                                 const mini_gnb_c_tx_grid_patch_t* patch,
                                                 uint64_t tx_index) {
  FILE* file = NULL;
  uint16_t tbsize = 0U;

  if (path == NULL || slot == NULL || patch == NULL || !patch->pdcch.valid) {
    return -1;
  }

  file = fopen(path, "wb");
  if (file == NULL) {
    return -1;
  }
  tbsize = mini_gnb_c_lookup_tbsize(patch->pdcch.scheduled_prb_len, patch->pdcch.mcs);

  fprintf(file,
          "direction=DL\n"
          "channel=PDCCH\n"
          "abs_slot=%d\n"
          "sfn=%u\n"
          "slot=%u\n"
          "type=DL_OBJ_PDCCH\n"
          "rnti=%u\n"
          "dci_format=%s\n"
          "dci_direction=%s\n"
          "coreset_prb_start=%u\n"
          "coreset_prb_len=%u\n"
          "scheduled_abs_slot=%d\n"
          "scheduled_type=%s\n"
          "scheduled_purpose=%s\n"
          "scheduled_prb_start=%u\n"
          "scheduled_prb_len=%u\n"
          "mcs=%u\n"
          "tbsize=%u\n"
          "k2=%d\n"
          "tx_index=%llu\n",
          slot->abs_slot,
          slot->sfn,
          slot->slot,
          patch->pdcch.rnti,
          mini_gnb_c_dci_format_to_string(patch->pdcch.format),
          mini_gnb_c_dci_direction_string(patch->pdcch.format),
          patch->pdcch.coreset_prb_start,
          patch->pdcch.coreset_prb_len,
          patch->pdcch.scheduled_abs_slot,
          mini_gnb_c_scheduled_object_name(&patch->pdcch),
          patch->pdcch.scheduled_ul_type == MINI_GNB_C_UL_BURST_DATA
              ? mini_gnb_c_ul_data_purpose_name(patch->pdcch.scheduled_ul_purpose)
              : "",
          patch->pdcch.scheduled_prb_start,
          patch->pdcch.scheduled_prb_len,
          patch->pdcch.mcs,
          tbsize,
          patch->pdcch.k2,
          (unsigned long long)tx_index);

  fclose(file);
  return 0;
}

static int mini_gnb_c_read_cf32_samples(const char* path,
                                        mini_gnb_c_complexf_t* out_samples,
                                        const size_t max_samples,
                                        size_t* out_sample_count) {
  FILE* file = NULL;
  long size_bytes = 0;
  size_t sample_count = 0;
  size_t i = 0;

  if (path == NULL || path[0] == '\0' || out_samples == NULL || out_sample_count == NULL || max_samples == 0U) {
    return -1;
  }

  file = fopen(path, "rb");
  if (file == NULL) {
    return -1;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return -1;
  }
  size_bytes = ftell(file);
  if (size_bytes <= 0 || (size_bytes % (long)(sizeof(float) * 2U)) != 0) {
    fclose(file);
    return -1;
  }
  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return -1;
  }

  sample_count = (size_t)size_bytes / (sizeof(float) * 2U);
  if (sample_count > max_samples) {
    fclose(file);
    return -1;
  }

  for (i = 0; i < sample_count; ++i) {
    float pair[2];
    if (fread(pair, sizeof(float), 2U, file) != 2U) {
      fclose(file);
      return -1;
    }
    out_samples[i].real = pair[0];
    out_samples[i].imag = pair[1];
  }

  fclose(file);
  *out_sample_count = sample_count;
  return 0;
}

static void mini_gnb_c_generate_ul_waveform(mini_gnb_c_complexf_t* out_samples,
                                            size_t* out_sample_count,
                                            const size_t sample_count,
                                            const double tone_step,
                                            const double amplitude) {
  size_t i = 0;

  if (out_samples == NULL || out_sample_count == NULL) {
    return;
  }

  for (i = 0; i < sample_count; ++i) {
    const double phase = tone_step * (double)i;
    out_samples[i].real = (float)(amplitude * cos(phase));
    out_samples[i].imag = (float)(amplitude * sin(phase));
  }
  *out_sample_count = sample_count;
}

static void mini_gnb_c_build_msg3_mac_pdu(const mini_gnb_c_sim_config_t* sim,
                                          const uint16_t tc_rnti,
                                          mini_gnb_c_buffer_t* out_mac_pdu) {
  uint8_t contention_id[16];
  uint8_t ue_identity[16];
  size_t contention_id_len = 0;
  size_t ue_identity_len = 0;
  mini_gnb_c_buffer_t ccch;

  if (sim == NULL || out_mac_pdu == NULL) {
    return;
  }

  mini_gnb_c_buffer_reset(out_mac_pdu);
  if (mini_gnb_c_hex_to_bytes(sim->contention_id_hex,
                              contention_id,
                              sizeof(contention_id),
                              &contention_id_len) != 0) {
    return;
  }
  if (mini_gnb_c_hex_to_bytes(sim->ue_identity_hex,
                              ue_identity,
                              sizeof(ue_identity),
                              &ue_identity_len) != 0) {
    return;
  }

  mini_gnb_c_buffer_reset(&ccch);
  memcpy(ccch.bytes, contention_id, contention_id_len);
  ccch.bytes[contention_id_len] = sim->establishment_cause;
  ccch.bytes[contention_id_len + 1U] = sim->ue_identity_type;
  memcpy(&ccch.bytes[contention_id_len + 2U], ue_identity, ue_identity_len);
  ccch.len = contention_id_len + 2U + ue_identity_len;

  if (sim->include_crnti_ce) {
    out_mac_pdu->bytes[out_mac_pdu->len++] = 2U;
    out_mac_pdu->bytes[out_mac_pdu->len++] = 2U;
    out_mac_pdu->bytes[out_mac_pdu->len++] = (uint8_t)(tc_rnti & 0xFFU);
    out_mac_pdu->bytes[out_mac_pdu->len++] = (uint8_t)((tc_rnti >> 8U) & 0xFFU);
  }

  out_mac_pdu->bytes[out_mac_pdu->len++] = 1U;
  out_mac_pdu->bytes[out_mac_pdu->len++] = (uint8_t)ccch.len;
  memcpy(&out_mac_pdu->bytes[out_mac_pdu->len], ccch.bytes, ccch.len);
  out_mac_pdu->len += ccch.len;
}

static void mini_gnb_c_build_ul_data_mac_pdu(const mini_gnb_c_sim_config_t* sim,
                                             const mini_gnb_c_ul_data_purpose_t purpose,
                                             mini_gnb_c_buffer_t* out_mac_pdu) {
  char bsr_text[MINI_GNB_C_MAX_TEXT];
  size_t ul_data_len = 0;

  if (sim == NULL || out_mac_pdu == NULL) {
    return;
  }

  mini_gnb_c_buffer_reset(out_mac_pdu);
  if (purpose == MINI_GNB_C_UL_DATA_PURPOSE_BSR) {
    if (snprintf(bsr_text,
                 sizeof(bsr_text),
                 "BSR|bytes=%d",
                 sim->ul_bsr_buffer_size_bytes) >= (int)sizeof(bsr_text)) {
      return;
    }
    (void)mini_gnb_c_buffer_set_text(out_mac_pdu, bsr_text);
    return;
  }

  if (mini_gnb_c_hex_to_bytes(sim->ul_data_hex,
                              out_mac_pdu->bytes,
                              sizeof(out_mac_pdu->bytes),
                              &ul_data_len) != 0) {
    return;
  }
  out_mac_pdu->len = ul_data_len;
}

static void mini_gnb_c_generate_slot_text_samples(mini_gnb_c_ul_burst_type_t type,
                                                  mini_gnb_c_complexf_t* out_samples,
                                                  size_t* out_sample_count,
                                                  size_t requested_sample_count) {
  size_t sample_count = requested_sample_count;

  if (sample_count == 0U || sample_count > MINI_GNB_C_MAX_IQ_SAMPLES) {
    sample_count = (type == MINI_GNB_C_UL_BURST_PRACH)
                       ? 256U
                       : (type == MINI_GNB_C_UL_BURST_MSG3
                              ? 320U
                              : (type == MINI_GNB_C_UL_BURST_PUCCH_SR ? 96U : 288U));
  }

  mini_gnb_c_generate_ul_waveform(out_samples,
                                  out_sample_count,
                                  sample_count,
                                  (type == MINI_GNB_C_UL_BURST_PRACH)
                                      ? 0.0975
                                      : (type == MINI_GNB_C_UL_BURST_MSG3
                                             ? 0.1425
                                             : (type == MINI_GNB_C_UL_BURST_PUCCH_SR ? 0.215 : 0.1765)),
                                  (type == MINI_GNB_C_UL_BURST_PRACH)
                                      ? 0.35
                                      : (type == MINI_GNB_C_UL_BURST_MSG3
                                             ? 0.25
                                             : (type == MINI_GNB_C_UL_BURST_PUCCH_SR ? 0.18 : 0.22)));
}

static void mini_gnb_c_fill_ul_burst(mini_gnb_c_radio_burst_t* out_burst,
                                     const mini_gnb_c_ul_burst_type_t type,
                                     const mini_gnb_c_complexf_t* samples,
                                     const size_t sample_count) {
  if (out_burst == NULL || samples == NULL || sample_count > MINI_GNB_C_MAX_IQ_SAMPLES) {
    return;
  }

  out_burst->ul_type = type;
  out_burst->nof_samples = (uint32_t)sample_count;
  memcpy(out_burst->samples, samples, sample_count * sizeof(samples[0]));
}

static void mini_gnb_c_fill_burst_payload_from_transport(const char* text,
                                                         const mini_gnb_c_buffer_t* default_mac_pdu,
                                                         mini_gnb_c_buffer_t* out_mac_pdu) {
  char payload_hex[MINI_GNB_C_MAX_PAYLOAD * 2U + 1U];
  char payload_text[MINI_GNB_C_MAX_PAYLOAD + 1U];
  size_t payload_len = 0;

  if (text == NULL || out_mac_pdu == NULL) {
    return;
  }

  mini_gnb_c_buffer_reset(out_mac_pdu);
  if (mini_gnb_c_extract_transport_value_any(text, "payload_hex", "mac_pdu_hex", payload_hex, sizeof(payload_hex)) ==
          0 &&
      mini_gnb_c_hex_to_bytes(payload_hex,
                              out_mac_pdu->bytes,
                              sizeof(out_mac_pdu->bytes),
                              &payload_len) == 0) {
    out_mac_pdu->len = payload_len;
    return;
  }

  if (mini_gnb_c_extract_transport_value(text, "payload_text", payload_text, sizeof(payload_text)) == 0) {
    (void)mini_gnb_c_buffer_set_text(out_mac_pdu, payload_text);
    return;
  }

  if (default_mac_pdu != NULL) {
    *out_mac_pdu = *default_mac_pdu;
  }
}

static bool mini_gnb_c_try_receive_local_exchange_event(mini_gnb_c_mock_radio_frontend_t* radio,
                                                        const mini_gnb_c_slot_indication_t* slot,
                                                        mini_gnb_c_radio_burst_t* out_burst) {
  char event_path[MINI_GNB_C_MAX_PATH];
  char type_value[32];
  char payload_hex[MINI_GNB_C_MAX_PAYLOAD * 2U + 1U];
  char payload_text[MINI_GNB_C_MAX_PAYLOAD + 1U];
  char* text = NULL;
  mini_gnb_c_complexf_t samples[MINI_GNB_C_MAX_IQ_SAMPLES];
  size_t sample_count = 0U;
  int int_value = 0;
  size_t payload_len = 0U;
  double double_value = 0.0;

  if (radio == NULL || slot == NULL || out_burst == NULL || !radio->local_exchange_mode_enabled) {
    return false;
  }

  while (mini_gnb_c_find_local_exchange_event_path(radio->sim.local_exchange_dir,
                                                   "ue_to_gnb",
                                                   "ue",
                                                   radio->local_exchange_next_sequence,
                                                   event_path,
                                                   sizeof(event_path)) == 0) {
    text = mini_gnb_c_read_text_file(event_path);
    if (text == NULL) {
      return false;
    }
    if (mini_gnb_c_extract_json_int(text, "abs_slot", &int_value) != 0) {
      free(text);
      return false;
    }
    if (slot->abs_slot < int_value) {
      free(text);
      return false;
    }
    if (slot->abs_slot > int_value) {
      ++radio->local_exchange_next_sequence;
      free(text);
      continue;
    }
    if (mini_gnb_c_extract_json_string(text, "type", type_value, sizeof(type_value)) != 0) {
      free(text);
      return false;
    }

    if (strcmp(type_value, "PRACH") == 0) {
      mini_gnb_c_generate_slot_text_samples(MINI_GNB_C_UL_BURST_PRACH, samples, &sample_count, 0U);
      mini_gnb_c_fill_ul_burst(out_burst, MINI_GNB_C_UL_BURST_PRACH, samples, sample_count);
      out_burst->preamble_id = (mini_gnb_c_extract_json_int(text, "preamble_id", &int_value) == 0)
                                   ? (uint8_t)int_value
                                   : radio->sim.preamble_id;
      out_burst->ta_est =
          (mini_gnb_c_extract_json_int(text, "ta_est", &int_value) == 0) ? int_value : radio->sim.ta_est;
      out_burst->peak_metric = (mini_gnb_c_extract_json_double(text, "peak_metric", &double_value) == 0)
                                   ? double_value
                                   : radio->sim.peak_metric;
      out_burst->snr_db = 20.0;
    } else if (strcmp(type_value, "MSG3") == 0) {
      mini_gnb_c_generate_slot_text_samples(MINI_GNB_C_UL_BURST_MSG3, samples, &sample_count, 0U);
      mini_gnb_c_fill_ul_burst(out_burst, MINI_GNB_C_UL_BURST_MSG3, samples, sample_count);
      out_burst->rnti = (mini_gnb_c_extract_json_int(text, "rnti", &int_value) == 0)
                            ? (uint16_t)int_value
                            : (radio->msg3_armed ? radio->msg3_rnti : 0U);
      out_burst->snr_db = radio->sim.msg3_snr_db;
      out_burst->evm = radio->sim.msg3_evm;
      if (mini_gnb_c_extract_json_string(text, "payload_hex", payload_hex, sizeof(payload_hex)) == 0 &&
          mini_gnb_c_hex_to_bytes(payload_hex, out_burst->mac_pdu.bytes, sizeof(out_burst->mac_pdu.bytes), &payload_len) ==
              0) {
        out_burst->mac_pdu.len = payload_len;
      } else {
        out_burst->mac_pdu = radio->msg3_mac_pdu;
      }
    } else if (strcmp(type_value, "PUCCH_SR") == 0) {
      mini_gnb_c_generate_slot_text_samples(MINI_GNB_C_UL_BURST_PUCCH_SR, samples, &sample_count, 0U);
      mini_gnb_c_fill_ul_burst(out_burst, MINI_GNB_C_UL_BURST_PUCCH_SR, samples, sample_count);
      out_burst->rnti = (mini_gnb_c_extract_json_int(text, "rnti", &int_value) == 0)
                            ? (uint16_t)int_value
                            : (radio->pucch_sr_armed ? radio->pucch_sr_rnti : 0U);
      out_burst->snr_db = 14.0;
      out_burst->evm = 1.2;
      out_burst->crc_ok_override_valid = true;
      out_burst->crc_ok_override = true;
    } else if (strcmp(type_value, "BSR") == 0 || strcmp(type_value, "DATA") == 0) {
      mini_gnb_c_generate_slot_text_samples(MINI_GNB_C_UL_BURST_DATA, samples, &sample_count, 0U);
      mini_gnb_c_fill_ul_burst(out_burst, MINI_GNB_C_UL_BURST_DATA, samples, sample_count);
      out_burst->rnti = (mini_gnb_c_extract_json_int(text, "rnti", &int_value) == 0)
                            ? (uint16_t)int_value
                            : (radio->ul_data_armed ? radio->ul_data_rnti : 0U);
      out_burst->snr_db = radio->sim.ul_data_snr_db;
      out_burst->evm = radio->sim.ul_data_evm;
      out_burst->tbsize = radio->ul_data_tbsize;
      out_burst->crc_ok_override_valid = true;
      out_burst->crc_ok_override = true;
      if (mini_gnb_c_extract_json_string(text, "payload_hex", payload_hex, sizeof(payload_hex)) == 0 &&
          mini_gnb_c_hex_to_bytes(payload_hex, out_burst->mac_pdu.bytes, sizeof(out_burst->mac_pdu.bytes), &payload_len) ==
              0) {
        out_burst->mac_pdu.len = payload_len;
      } else if (mini_gnb_c_extract_json_string(text, "payload_text", payload_text, sizeof(payload_text)) == 0) {
        (void)mini_gnb_c_buffer_set_text(&out_burst->mac_pdu, payload_text);
      } else {
        out_burst->mac_pdu = radio->ul_data_mac_pdu;
      }
    } else {
      free(text);
      return false;
    }

    ++radio->local_exchange_next_sequence;
    free(text);
    return true;
  }

  return false;
}

static bool mini_gnb_c_try_receive_slot_text_object(mini_gnb_c_mock_radio_frontend_t* radio,
                                                    const mini_gnb_c_slot_indication_t* slot,
                                                    const char* object_name,
                                                    mini_gnb_c_radio_burst_t* out_burst) {
  char path[MINI_GNB_C_MAX_PATH];
  char* text = NULL;
  char type_value[64];
  char direction_value[16];
  mini_gnb_c_complexf_t samples[MINI_GNB_C_MAX_IQ_SAMPLES];
  size_t sample_count = 0;
  int int_value = 0;
  double double_value = 0.0;
  bool bool_value = false;

  if (radio == NULL || slot == NULL || object_name == NULL || out_burst == NULL) {
    return false;
  }

  if (mini_gnb_c_build_slot_object_path(radio->sim.ul_input_dir,
                                        slot->abs_slot,
                                        "UL",
                                        object_name,
                                        "txt",
                                        path,
                                        sizeof(path)) != 0) {
    return false;
  }

  text = mini_gnb_c_read_text_file(path);
  if (text == NULL) {
    return false;
  }

  if (mini_gnb_c_extract_transport_value(text, "direction", direction_value, sizeof(direction_value)) == 0 &&
      strcmp(direction_value, "UL") != 0) {
    free(text);
    return false;
  }

  if (strcmp(object_name, "PRACH") == 0) {
    if (mini_gnb_c_extract_transport_value(text, "type", type_value, sizeof(type_value)) == 0 &&
        strcmp(type_value, "PRACH") != 0 && strcmp(type_value, "UL_OBJ_PRACH") != 0) {
      free(text);
      return false;
    }
    if (mini_gnb_c_extract_transport_int(text, "sample_count", &int_value) == 0 && int_value > 0) {
      sample_count = (size_t)int_value;
    }
    mini_gnb_c_generate_slot_text_samples(MINI_GNB_C_UL_BURST_PRACH, samples, &sample_count, sample_count);
    mini_gnb_c_fill_ul_burst(out_burst, MINI_GNB_C_UL_BURST_PRACH, samples, sample_count);
    out_burst->preamble_id = (mini_gnb_c_extract_transport_int(text, "preamble_id", &int_value) == 0)
                                 ? (uint8_t)int_value
                                 : radio->sim.preamble_id;
    out_burst->ta_est = (mini_gnb_c_extract_transport_int(text, "ta_est", &int_value) == 0) ? int_value
                                                                                              : radio->sim.ta_est;
    out_burst->peak_metric = (mini_gnb_c_extract_transport_double(text, "peak_metric", &double_value) == 0)
                                 ? double_value
                                 : radio->sim.peak_metric;
    out_burst->snr_db =
        (mini_gnb_c_extract_transport_double(text, "snr_db", &double_value) == 0) ? double_value : 20.0;
    free(text);
    return true;
  }

  if (strcmp(object_name, "MSG3") == 0) {
    if (mini_gnb_c_extract_transport_value(text, "type", type_value, sizeof(type_value)) == 0 &&
        strcmp(type_value, "MSG3") != 0 && strcmp(type_value, "UL_OBJ_MSG3") != 0) {
      free(text);
      return false;
    }
    if (mini_gnb_c_extract_transport_int(text, "sample_count", &int_value) == 0 && int_value > 0) {
      sample_count = (size_t)int_value;
    }
    mini_gnb_c_generate_slot_text_samples(MINI_GNB_C_UL_BURST_MSG3, samples, &sample_count, sample_count);
    mini_gnb_c_fill_ul_burst(out_burst, MINI_GNB_C_UL_BURST_MSG3, samples, sample_count);
    out_burst->rnti = (mini_gnb_c_extract_transport_int(text, "rnti", &int_value) == 0)
                          ? (uint16_t)int_value
                          : (radio->msg3_armed ? radio->msg3_rnti : 0U);
    out_burst->snr_db = (mini_gnb_c_extract_transport_double(text, "snr_db", &double_value) == 0)
                            ? double_value
                            : radio->sim.msg3_snr_db;
    out_burst->evm = (mini_gnb_c_extract_transport_double(text, "evm", &double_value) == 0) ? double_value
                                                                                               : radio->sim.msg3_evm;
    if (mini_gnb_c_extract_transport_bool(text, "crc_ok", &bool_value) == 0) {
      out_burst->crc_ok_override_valid = true;
      out_burst->crc_ok_override = bool_value;
    }
    mini_gnb_c_fill_burst_payload_from_transport(text, &radio->msg3_mac_pdu, &out_burst->mac_pdu);

    free(text);
    return true;
  }

  if (strcmp(object_name, "PUCCH_SR") == 0) {
    if (mini_gnb_c_extract_transport_value(text, "type", type_value, sizeof(type_value)) == 0 &&
        strcmp(type_value, "PUCCH_SR") != 0 && strcmp(type_value, "UL_OBJ_PUCCH_SR") != 0 &&
        strcmp(type_value, "UL_OBJ_SR") != 0 && strcmp(type_value, "UL_OBJ_PUCCH") != 0) {
      free(text);
      return false;
    }
    if (mini_gnb_c_extract_transport_int(text, "sample_count", &int_value) == 0 && int_value > 0) {
      sample_count = (size_t)int_value;
    }
    mini_gnb_c_generate_slot_text_samples(MINI_GNB_C_UL_BURST_PUCCH_SR, samples, &sample_count, sample_count);
    mini_gnb_c_fill_ul_burst(out_burst, MINI_GNB_C_UL_BURST_PUCCH_SR, samples, sample_count);
    out_burst->rnti = (mini_gnb_c_extract_transport_int(text, "rnti", &int_value) == 0)
                          ? (uint16_t)int_value
                          : (radio->pucch_sr_armed ? radio->pucch_sr_rnti : 0U);
    out_burst->snr_db = (mini_gnb_c_extract_transport_double(text, "snr_db", &double_value) == 0)
                            ? double_value
                            : 14.0;
    out_burst->evm = (mini_gnb_c_extract_transport_double(text, "evm", &double_value) == 0)
                         ? double_value
                         : 1.2;
    if (mini_gnb_c_extract_transport_bool(text, "crc_ok", &bool_value) == 0) {
      out_burst->crc_ok_override_valid = true;
      out_burst->crc_ok_override = bool_value;
    } else {
      out_burst->crc_ok_override_valid = true;
      out_burst->crc_ok_override = true;
    }
    free(text);
    return true;
  }

  if (strcmp(object_name, "DATA") == 0) {
    if (mini_gnb_c_extract_transport_value(text, "type", type_value, sizeof(type_value)) == 0 &&
        strcmp(type_value, "DATA") != 0 && strcmp(type_value, "UL_OBJ_DATA") != 0 &&
        strcmp(type_value, "UL_OBJ_PUSCH") != 0) {
      free(text);
      return false;
    }
    if (mini_gnb_c_extract_transport_int(text, "sample_count", &int_value) == 0 && int_value > 0) {
      sample_count = (size_t)int_value;
    }
    mini_gnb_c_generate_slot_text_samples(MINI_GNB_C_UL_BURST_DATA, samples, &sample_count, sample_count);
    mini_gnb_c_fill_ul_burst(out_burst, MINI_GNB_C_UL_BURST_DATA, samples, sample_count);
    out_burst->rnti = (mini_gnb_c_extract_transport_int(text, "rnti", &int_value) == 0)
                          ? (uint16_t)int_value
                          : (radio->ul_data_armed ? radio->ul_data_rnti : 0U);
    out_burst->snr_db = (mini_gnb_c_extract_transport_double(text, "snr_db", &double_value) == 0)
                            ? double_value
                            : radio->sim.ul_data_snr_db;
    out_burst->evm = (mini_gnb_c_extract_transport_double(text, "evm", &double_value) == 0)
                         ? double_value
                         : radio->sim.ul_data_evm;
    if (mini_gnb_c_extract_transport_bool(text, "crc_ok", &bool_value) == 0) {
      out_burst->crc_ok_override_valid = true;
      out_burst->crc_ok_override = bool_value;
    }
    mini_gnb_c_fill_burst_payload_from_transport(text, &radio->ul_data_mac_pdu, &out_burst->mac_pdu);
    if (mini_gnb_c_extract_transport_int(text, "tbsize", &int_value) == 0 && int_value > 0) {
      out_burst->tbsize = (uint16_t)int_value;
    } else {
      out_burst->tbsize = radio->ul_data_tbsize;
    }

    free(text);
    return true;
  }

  free(text);
  return false;
}

static bool mini_gnb_c_try_receive_slot_input(mini_gnb_c_mock_radio_frontend_t* radio,
                                              const mini_gnb_c_slot_indication_t* slot,
                                              mini_gnb_c_radio_burst_t* out_burst) {
  char path[MINI_GNB_C_MAX_PATH];
  mini_gnb_c_complexf_t samples[MINI_GNB_C_MAX_IQ_SAMPLES];
  size_t sample_count = 0;

  if (radio == NULL || slot == NULL || out_burst == NULL || !radio->slot_input_mode_enabled) {
    return false;
  }

  if (mini_gnb_c_try_receive_slot_text_object(radio, slot, "PRACH", out_burst)) {
    return true;
  }

  if (mini_gnb_c_build_slot_object_path(radio->sim.ul_input_dir,
                                       slot->abs_slot,
                                       "UL",
                                       "PRACH",
                                       "cf32",
                                       path,
                                       sizeof(path)) == 0 &&
      mini_gnb_c_read_cf32_samples(path, samples, MINI_GNB_C_MAX_IQ_SAMPLES, &sample_count) == 0) {
    mini_gnb_c_fill_ul_burst(out_burst, MINI_GNB_C_UL_BURST_PRACH, samples, sample_count);
    out_burst->preamble_id = radio->sim.preamble_id;
    out_burst->ta_est = radio->sim.ta_est;
    out_burst->peak_metric = radio->sim.peak_metric;
    out_burst->snr_db = 20.0;
    return true;
  }

  if (mini_gnb_c_try_receive_slot_text_object(radio, slot, "MSG3", out_burst)) {
    return true;
  }

  if (mini_gnb_c_build_slot_object_path(radio->sim.ul_input_dir,
                                       slot->abs_slot,
                                       "UL",
                                       "MSG3",
                                       "cf32",
                                       path,
                                       sizeof(path)) == 0 &&
      mini_gnb_c_read_cf32_samples(path, samples, MINI_GNB_C_MAX_IQ_SAMPLES, &sample_count) == 0) {
    mini_gnb_c_fill_ul_burst(out_burst, MINI_GNB_C_UL_BURST_MSG3, samples, sample_count);
    out_burst->rnti = radio->msg3_armed ? radio->msg3_rnti : 0U;
    out_burst->snr_db = radio->sim.msg3_snr_db;
    out_burst->evm = radio->sim.msg3_evm;
    out_burst->mac_pdu = radio->msg3_mac_pdu;
    return true;
  }

  if (mini_gnb_c_try_receive_slot_text_object(radio, slot, "PUCCH_SR", out_burst)) {
    return true;
  }

  if (mini_gnb_c_build_slot_object_path(radio->sim.ul_input_dir,
                                       slot->abs_slot,
                                       "UL",
                                       "PUCCH_SR",
                                       "cf32",
                                       path,
                                       sizeof(path)) == 0 &&
      mini_gnb_c_read_cf32_samples(path, samples, MINI_GNB_C_MAX_IQ_SAMPLES, &sample_count) == 0) {
    mini_gnb_c_fill_ul_burst(out_burst, MINI_GNB_C_UL_BURST_PUCCH_SR, samples, sample_count);
    out_burst->rnti = radio->pucch_sr_armed ? radio->pucch_sr_rnti : 0U;
    out_burst->snr_db = 14.0;
    out_burst->evm = 1.2;
    out_burst->crc_ok_override_valid = true;
    out_burst->crc_ok_override = true;
    return true;
  }

  if (mini_gnb_c_try_receive_slot_text_object(radio, slot, "DATA", out_burst)) {
    return true;
  }

  if (mini_gnb_c_build_slot_object_path(radio->sim.ul_input_dir,
                                       slot->abs_slot,
                                       "UL",
                                       "DATA",
                                       "cf32",
                                       path,
                                       sizeof(path)) == 0 &&
      mini_gnb_c_read_cf32_samples(path, samples, MINI_GNB_C_MAX_IQ_SAMPLES, &sample_count) == 0) {
    mini_gnb_c_fill_ul_burst(out_burst, MINI_GNB_C_UL_BURST_DATA, samples, sample_count);
    out_burst->rnti = radio->ul_data_armed ? radio->ul_data_rnti : 0U;
    out_burst->snr_db = radio->sim.ul_data_snr_db;
    out_burst->evm = radio->sim.ul_data_evm;
    out_burst->tbsize = radio->ul_data_tbsize;
    out_burst->mac_pdu = radio->ul_data_mac_pdu;
    return true;
  }

  return false;
}

void mini_gnb_c_mock_radio_frontend_init(mini_gnb_c_mock_radio_frontend_t* radio,
                                         const mini_gnb_c_rf_config_t* rf_config,
                                         const mini_gnb_c_sim_config_t* sim_config) {
  if (radio == NULL || rf_config == NULL || sim_config == NULL) {
    return;
  }

  memset(radio, 0, sizeof(*radio));
  memcpy(&radio->config, rf_config, sizeof(*rf_config));
  memcpy(&radio->sim, sim_config, sizeof(*sim_config));
  radio->retry_prach_abs_slot = -1;
  radio->local_exchange_next_sequence = 1u;

  if (sim_config->ul_input_dir[0] != '\0') {
    char resolved_input_dir[MINI_GNB_C_MAX_PATH];
    if (mini_gnb_c_resolve_input_dir(sim_config->ul_input_dir,
                                     resolved_input_dir,
                                     sizeof(resolved_input_dir)) == 0) {
      (void)snprintf(radio->sim.ul_input_dir, sizeof(radio->sim.ul_input_dir), "%s", resolved_input_dir);
      radio->slot_input_mode_enabled = mini_gnb_c_directory_exists(radio->sim.ul_input_dir);
    }
  }
  if (sim_config->local_exchange_dir[0] != '\0') {
    char resolved_exchange_dir[MINI_GNB_C_MAX_PATH];
    char ue_to_gnb_dir[MINI_GNB_C_MAX_PATH];

    if (mini_gnb_c_resolve_input_dir(sim_config->local_exchange_dir,
                                     resolved_exchange_dir,
                                     sizeof(resolved_exchange_dir)) == 0) {
      (void)snprintf(radio->sim.local_exchange_dir,
                     sizeof(radio->sim.local_exchange_dir),
                     "%s",
                     resolved_exchange_dir);
      if (mini_gnb_c_build_local_exchange_channel_dir(radio->sim.local_exchange_dir,
                                                      "ue_to_gnb",
                                                      ue_to_gnb_dir,
                                                      sizeof(ue_to_gnb_dir)) == 0) {
        radio->local_exchange_mode_enabled = mini_gnb_c_directory_exists(ue_to_gnb_dir);
      }
    }
  }

  if (mini_gnb_c_read_cf32_samples(sim_config->ul_prach_cf32_path,
                                   radio->prach_samples,
                                   MINI_GNB_C_MAX_IQ_SAMPLES,
                                   &radio->prach_sample_count) != 0) {
    mini_gnb_c_generate_ul_waveform(radio->prach_samples,
                                    &radio->prach_sample_count,
                                    256U,
                                    0.0975,
                                    0.35);
  }

  if (mini_gnb_c_read_cf32_samples(sim_config->ul_msg3_cf32_path,
                                   radio->msg3_samples,
                                   MINI_GNB_C_MAX_IQ_SAMPLES,
                                   &radio->msg3_sample_count) != 0) {
    mini_gnb_c_generate_ul_waveform(radio->msg3_samples,
                                    &radio->msg3_sample_count,
                                    320U,
                                    0.1425,
                                    0.25);
  }

  mini_gnb_c_generate_ul_waveform(radio->pucch_sr_samples,
                                  &radio->pucch_sr_sample_count,
                                  96U,
                                  0.215,
                                  0.18);
  mini_gnb_c_generate_ul_waveform(radio->ul_data_samples,
                                  &radio->ul_data_sample_count,
                                  288U,
                                  0.1765,
                                  0.22);
  radio->ul_data_purpose = MINI_GNB_C_UL_DATA_PURPOSE_PAYLOAD;
  mini_gnb_c_build_ul_data_mac_pdu(&radio->sim, radio->ul_data_purpose, &radio->ul_data_mac_pdu);
}

void mini_gnb_c_mock_radio_frontend_receive(mini_gnb_c_mock_radio_frontend_t* radio,
                                            const mini_gnb_c_slot_indication_t* slot,
                                            mini_gnb_c_radio_burst_t* out_burst) {
  if (radio == NULL || slot == NULL || out_burst == NULL) {
    return;
  }

  memset(out_burst, 0, sizeof(*out_burst));
  radio->last_hw_time_ns = slot->slot_start_ns;
  out_burst->hw_time_ns = slot->slot_start_ns;
  out_burst->sfn = slot->sfn;
  out_burst->slot = slot->slot;
  out_burst->status.hw_time_ns = slot->slot_start_ns;

  if (radio->local_exchange_mode_enabled) {
    if (mini_gnb_c_try_receive_local_exchange_event(radio, slot, out_burst)) {
      if (out_burst->ul_type == MINI_GNB_C_UL_BURST_MSG3 && radio->msg3_armed &&
          slot->abs_slot >= radio->msg3_abs_slot) {
        radio->msg3_armed = false;
      }
      if (out_burst->ul_type == MINI_GNB_C_UL_BURST_PUCCH_SR && radio->pucch_sr_armed &&
          slot->abs_slot >= radio->pucch_sr_abs_slot) {
        radio->pucch_sr_armed = false;
      }
      if (out_burst->ul_type == MINI_GNB_C_UL_BURST_DATA && radio->ul_data_armed &&
          slot->abs_slot >= radio->ul_data_abs_slot) {
        radio->ul_data_armed = false;
      }
      return;
    }

    if (radio->msg3_armed && slot->abs_slot > radio->msg3_abs_slot) {
      radio->msg3_armed = false;
    }
    if (radio->pucch_sr_armed && slot->abs_slot > radio->pucch_sr_abs_slot) {
      radio->pucch_sr_armed = false;
    }
    if (radio->ul_data_armed && slot->abs_slot > radio->ul_data_abs_slot) {
      radio->ul_data_armed = false;
    }
    if (radio->retry_prach_armed && slot->abs_slot > radio->retry_prach_abs_slot) {
      radio->retry_prach_armed = false;
      radio->retry_prach_abs_slot = -1;
    }
    return;
  }

  if (radio->slot_input_mode_enabled) {
    if (mini_gnb_c_try_receive_slot_input(radio, slot, out_burst)) {
      if (out_burst->ul_type == MINI_GNB_C_UL_BURST_MSG3 && radio->msg3_armed &&
          slot->abs_slot >= radio->msg3_abs_slot) {
        radio->msg3_armed = false;
      }
      if (out_burst->ul_type == MINI_GNB_C_UL_BURST_PUCCH_SR && radio->pucch_sr_armed &&
          slot->abs_slot >= radio->pucch_sr_abs_slot) {
        radio->pucch_sr_armed = false;
      }
      if (out_burst->ul_type == MINI_GNB_C_UL_BURST_DATA && radio->ul_data_armed &&
          slot->abs_slot >= radio->ul_data_abs_slot) {
        radio->ul_data_armed = false;
      }
      return;
    }

    if (radio->msg3_armed && slot->abs_slot > radio->msg3_abs_slot) {
      radio->msg3_armed = false;
    }
    if (radio->pucch_sr_armed && slot->abs_slot > radio->pucch_sr_abs_slot) {
      radio->pucch_sr_armed = false;
    }
    if (radio->ul_data_armed && slot->abs_slot > radio->ul_data_abs_slot) {
      radio->ul_data_armed = false;
    }
    if (radio->retry_prach_armed && slot->abs_slot > radio->retry_prach_abs_slot) {
      radio->retry_prach_armed = false;
      radio->retry_prach_abs_slot = -1;
    }
    return;
  }

  if (!radio->initial_prach_emitted && slot->abs_slot == radio->sim.prach_trigger_abs_slot) {
    radio->initial_prach_emitted = true;
    mini_gnb_c_fill_ul_burst(out_burst,
                             MINI_GNB_C_UL_BURST_PRACH,
                             radio->prach_samples,
                             radio->prach_sample_count);
    out_burst->preamble_id = radio->sim.preamble_id;
    out_burst->ta_est = radio->sim.ta_est;
    out_burst->peak_metric = radio->sim.peak_metric;
    out_burst->snr_db = 20.0;
    return;
  }

  if (radio->msg3_armed && slot->abs_slot == radio->msg3_abs_slot && radio->sim.msg3_present) {
    radio->msg3_armed = false;
    mini_gnb_c_fill_ul_burst(out_burst,
                             MINI_GNB_C_UL_BURST_MSG3,
                             radio->msg3_samples,
                             radio->msg3_sample_count);
    out_burst->rnti = radio->msg3_rnti;
    out_burst->snr_db = radio->sim.msg3_snr_db;
    out_burst->evm = radio->sim.msg3_evm;
    out_burst->mac_pdu = radio->msg3_mac_pdu;
    return;
  }

  if (radio->msg3_armed && slot->abs_slot > radio->msg3_abs_slot) {
    radio->msg3_armed = false;
  }

  if (radio->pucch_sr_armed && slot->abs_slot == radio->pucch_sr_abs_slot && radio->sim.ul_data_present) {
    radio->pucch_sr_armed = false;
    mini_gnb_c_fill_ul_burst(out_burst,
                             MINI_GNB_C_UL_BURST_PUCCH_SR,
                             radio->pucch_sr_samples,
                             radio->pucch_sr_sample_count);
    out_burst->rnti = radio->pucch_sr_rnti;
    out_burst->snr_db = 14.0;
    out_burst->evm = 1.2;
    out_burst->crc_ok_override_valid = true;
    out_burst->crc_ok_override = true;
    return;
  }

  if (radio->pucch_sr_armed && slot->abs_slot > radio->pucch_sr_abs_slot) {
    radio->pucch_sr_armed = false;
  }

  if (radio->ul_data_armed && slot->abs_slot == radio->ul_data_abs_slot && radio->sim.ul_data_present) {
    radio->ul_data_armed = false;
    mini_gnb_c_fill_ul_burst(out_burst,
                             MINI_GNB_C_UL_BURST_DATA,
                             radio->ul_data_samples,
                             radio->ul_data_sample_count);
    out_burst->rnti = radio->ul_data_rnti;
    out_burst->snr_db = radio->sim.ul_data_snr_db;
    out_burst->evm = radio->sim.ul_data_evm;
    out_burst->tbsize = radio->ul_data_tbsize;
    out_burst->crc_ok_override_valid = true;
    out_burst->crc_ok_override = radio->sim.ul_data_crc_ok;
    out_burst->mac_pdu = radio->ul_data_mac_pdu;
    return;
  }

  if (radio->ul_data_armed && slot->abs_slot > radio->ul_data_abs_slot) {
    radio->ul_data_armed = false;
  }

  if (radio->retry_prach_armed && slot->abs_slot == radio->retry_prach_abs_slot) {
    radio->retry_prach_armed = false;
    mini_gnb_c_fill_ul_burst(out_burst,
                             MINI_GNB_C_UL_BURST_PRACH,
                             radio->prach_samples,
                             radio->prach_sample_count);
    out_burst->preamble_id = radio->sim.preamble_id;
    out_burst->ta_est = radio->sim.ta_est;
    out_burst->peak_metric = radio->sim.peak_metric;
    out_burst->snr_db = 20.0;
  }
}

void mini_gnb_c_mock_radio_frontend_arm_msg3(mini_gnb_c_mock_radio_frontend_t* radio,
                                             const mini_gnb_c_ul_grant_for_msg3_t* ul_grant) {
  if (radio == NULL || ul_grant == NULL) {
    return;
  }

  radio->msg3_armed = true;
  radio->msg3_abs_slot = ul_grant->abs_slot;
  radio->msg3_rnti = ul_grant->tc_rnti;
  mini_gnb_c_build_msg3_mac_pdu(&radio->sim, radio->msg3_rnti, &radio->msg3_mac_pdu);

  if (!radio->sim.msg3_present && radio->sim.prach_retry_delay_slots >= 0) {
    radio->retry_prach_armed = true;
    radio->retry_prach_abs_slot = ul_grant->abs_slot + radio->sim.prach_retry_delay_slots;
  } else {
    radio->retry_prach_armed = false;
    radio->retry_prach_abs_slot = -1;
  }
}

void mini_gnb_c_mock_radio_frontend_arm_pucch_sr(mini_gnb_c_mock_radio_frontend_t* radio,
                                                 const uint16_t rnti,
                                                 const int abs_slot) {
  if (radio == NULL) {
    return;
  }

  radio->pucch_sr_armed = true;
  radio->pucch_sr_abs_slot = abs_slot;
  radio->pucch_sr_rnti = rnti;
}

void mini_gnb_c_mock_radio_frontend_arm_ul_data(mini_gnb_c_mock_radio_frontend_t* radio,
                                                const mini_gnb_c_ul_data_grant_t* ul_grant) {
  if (radio == NULL || ul_grant == NULL) {
    return;
  }

  radio->ul_data_armed = true;
  radio->ul_data_abs_slot = ul_grant->abs_slot;
  radio->ul_data_rnti = ul_grant->c_rnti;
  radio->ul_data_tbsize = mini_gnb_c_lookup_tbsize(ul_grant->prb_len, ul_grant->mcs);
  radio->ul_data_purpose = ul_grant->purpose;
  mini_gnb_c_build_ul_data_mac_pdu(&radio->sim, radio->ul_data_purpose, &radio->ul_data_mac_pdu);
}

void mini_gnb_c_mock_radio_frontend_submit_tx(mini_gnb_c_mock_radio_frontend_t* radio,
                                              const mini_gnb_c_slot_indication_t* slot,
                                              const mini_gnb_c_tx_grid_patch_t* patches,
                                              const size_t patch_count,
                                              mini_gnb_c_metrics_trace_t* metrics) {
  size_t i = 0;
  char iq_dir[MINI_GNB_C_MAX_PATH];
  char tx_dir[MINI_GNB_C_MAX_PATH];
  char details[MINI_GNB_C_MAX_EVENT_TEXT];

  if (radio == NULL || slot == NULL || patches == NULL || metrics == NULL) {
    return;
  }

  if (mini_gnb_c_join_path(metrics->output_dir, "iq", iq_dir, sizeof(iq_dir)) == 0) {
    (void)mini_gnb_c_ensure_directory_recursive(iq_dir);
  } else {
    iq_dir[0] = '\0';
  }
  if (mini_gnb_c_join_path(metrics->output_dir, "tx", tx_dir, sizeof(tx_dir)) == 0) {
    (void)mini_gnb_c_ensure_directory_recursive(tx_dir);
  } else {
    tx_dir[0] = '\0';
  }

  for (i = 0; i < patch_count; ++i) {
    const mini_gnb_c_tx_grid_patch_t* patch = &patches[i];
    char cf32_path[MINI_GNB_C_MAX_PATH];
    char json_path[MINI_GNB_C_MAX_PATH];
    char text_path[MINI_GNB_C_MAX_PATH];
    char pdcch_path[MINI_GNB_C_MAX_PATH];
    int export_ok = 0;
    int text_export_ok = 0;
    int pdcch_export_ok = 0;
    int written = 0;
    uint16_t tbsize = patch->pdcch.valid ? mini_gnb_c_lookup_tbsize(patch->prb_len, patch->pdcch.mcs) : 0U;

    ++radio->tx_burst_count;

    export_ok = (iq_dir[0] != '\0') &&
                (snprintf(cf32_path,
                          sizeof(cf32_path),
                          "%s/slot_%d_%s_rnti_%u.cf32",
                          iq_dir,
                          slot->abs_slot,
                          mini_gnb_c_dl_object_type_to_string(patch->type),
                          patch->rnti) < (int)sizeof(cf32_path)) &&
                (snprintf(json_path,
                          sizeof(json_path),
                          "%s/slot_%d_%s_rnti_%u.json",
                          iq_dir,
                          slot->abs_slot,
                          mini_gnb_c_dl_object_type_to_string(patch->type),
                          patch->rnti) < (int)sizeof(json_path)) &&
                (mini_gnb_c_write_cf32(cf32_path, patch) == 0) &&
                (mini_gnb_c_write_iq_metadata(json_path,
                                              cf32_path,
                                              slot,
                                              patch,
                                              radio->tx_burst_count) == 0);

    text_export_ok = (tx_dir[0] != '\0') &&
                     (mini_gnb_c_build_slot_object_path(tx_dir,
                                                        slot->abs_slot,
                                                        "DL",
                                                        mini_gnb_c_dl_transport_name(patch->type),
                                                        "txt",
                                                        text_path,
                                                        sizeof(text_path)) == 0) &&
                     (mini_gnb_c_write_transport_text(text_path,
                                                      export_ok ? cf32_path : "",
                                                      slot,
                                                      patch,
                                                      radio->tx_burst_count) == 0);

    pdcch_export_ok = patch->pdcch.valid && tx_dir[0] != '\0' &&
                      (mini_gnb_c_build_pdcch_transport_path(tx_dir,
                                                             slot,
                                                             &patch->pdcch,
                                                             pdcch_path,
                                                             sizeof(pdcch_path)) == 0) &&
                      (mini_gnb_c_write_pdcch_transport_text(pdcch_path,
                                                             slot,
                                                             patch,
                                                             radio->tx_burst_count) == 0);

    written = snprintf(details,
                       sizeof(details),
                       "type=%s,rnti=%u,prb_start=%u,prb_len=%u,tbsize=%u,payload_len=%zu,sample_count=%zu",
                       mini_gnb_c_dl_object_type_to_string(patch->type),
                       patch->rnti,
                       patch->prb_start,
                       patch->prb_len,
                       tbsize,
                       patch->payload_len,
                       patch->sample_count);
    if (export_ok && written >= 0 && (size_t)written < sizeof(details)) {
      size_t offset = (size_t)written;
      const char* suffix = ",iq_path=";
      size_t suffix_len = strlen(suffix);
      if (offset + suffix_len < sizeof(details)) {
        memcpy(details + offset, suffix, suffix_len);
        offset += suffix_len;
        details[offset] = '\0';
        if (offset + 1U < sizeof(details)) {
          size_t remaining = sizeof(details) - offset - 1U;
          size_t copy_len = strlen(cf32_path);
          if (copy_len > remaining) {
            copy_len = remaining;
          }
          memcpy(details + offset, cf32_path, copy_len);
          details[offset + copy_len] = '\0';
        }
      }
    }
    if (text_export_ok && strlen(details) + strlen(",tx_path=") + 1U < sizeof(details)) {
      size_t offset = strlen(details);
      size_t remaining = sizeof(details) - offset - 1U;
      size_t copy_len = strlen(text_path);
      memcpy(details + offset, ",tx_path=", strlen(",tx_path="));
      offset += strlen(",tx_path=");
      if (copy_len > remaining - strlen(",tx_path=")) {
        copy_len = remaining - strlen(",tx_path=");
      }
      memcpy(details + offset, text_path, copy_len);
      details[offset + copy_len] = '\0';
    }
    if (pdcch_export_ok && strlen(details) + strlen(",pdcch_path=") + 1U < sizeof(details)) {
      size_t offset = strlen(details);
      size_t remaining = sizeof(details) - offset - 1U;
      size_t copy_len = strlen(pdcch_path);
      memcpy(details + offset, ",pdcch_path=", strlen(",pdcch_path="));
      offset += strlen(",pdcch_path=");
      if (copy_len > remaining - strlen(",pdcch_path=")) {
        copy_len = remaining - strlen(",pdcch_path=");
      }
      memcpy(details + offset, pdcch_path, copy_len);
      details[offset + copy_len] = '\0';
    }
    mini_gnb_c_metrics_trace_event(metrics, "radio_tx", "Submitted DL burst.", slot->abs_slot, "%s", details);
  }
}

void mini_gnb_c_mock_radio_frontend_submit_pdcch(mini_gnb_c_mock_radio_frontend_t* radio,
                                                 const mini_gnb_c_slot_indication_t* slot,
                                                 const mini_gnb_c_pdcch_dci_t* pdcch,
                                                 mini_gnb_c_metrics_trace_t* metrics) {
  mini_gnb_c_tx_grid_patch_t patch;
  char tx_dir[MINI_GNB_C_MAX_PATH];
  char pdcch_path[MINI_GNB_C_MAX_PATH];

  if (radio == NULL || slot == NULL || pdcch == NULL || metrics == NULL || !pdcch->valid) {
    return;
  }

  memset(&patch, 0, sizeof(patch));
  patch.sfn = slot->sfn;
  patch.slot = slot->slot;
  patch.abs_slot = slot->abs_slot;
  patch.type = MINI_GNB_C_DL_OBJ_PDCCH;
  patch.rnti = pdcch->rnti;
  patch.pdcch = *pdcch;

  ++radio->tx_burst_count;

  if (mini_gnb_c_join_path(metrics->output_dir, "tx", tx_dir, sizeof(tx_dir)) != 0) {
    return;
  }
  (void)mini_gnb_c_ensure_directory_recursive(tx_dir);

  if (mini_gnb_c_build_pdcch_transport_path(tx_dir, slot, pdcch, pdcch_path, sizeof(pdcch_path)) == 0) {
    (void)mini_gnb_c_write_pdcch_transport_text(pdcch_path, slot, &patch, radio->tx_burst_count);
    mini_gnb_c_metrics_trace_event(metrics,
                                   "radio_tx",
                                   "Submitted standalone PDCCH.",
                                   slot->abs_slot,
                                   "type=DL_OBJ_PDCCH,rnti=%u,dci_format=%s,scheduled_type=%s,tbsize=%u,tx_path=%s",
                                   pdcch->rnti,
                                   mini_gnb_c_dci_format_to_string(pdcch->format),
                                   mini_gnb_c_scheduled_object_name(pdcch),
                                   mini_gnb_c_lookup_tbsize(pdcch->scheduled_prb_len, pdcch->mcs),
                                   pdcch_path);
  }
}
