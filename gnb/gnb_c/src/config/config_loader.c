#include "mini_gnb_c/config/config_loader.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mini_gnb_c/common/json_utils.h"

static char* mini_gnb_c_ltrim(char* text) {
  if (text == NULL) {
    return NULL;
  }

  while (*text != '\0' && isspace((unsigned char)*text)) {
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
  while (len > 0U && isspace((unsigned char)text[len - 1U])) {
    text[len - 1U] = '\0';
    --len;
  }
}

static void mini_gnb_c_strip_inline_comment(char* text) {
  bool in_single_quote = false;
  bool in_double_quote = false;
  size_t i = 0;

  if (text == NULL) {
    return;
  }

  for (i = 0; text[i] != '\0'; ++i) {
    if (text[i] == '\'' && !in_double_quote) {
      in_single_quote = !in_single_quote;
      continue;
    }
    if (text[i] == '"' && !in_single_quote) {
      in_double_quote = !in_double_quote;
      continue;
    }
    if (text[i] == '#' && !in_single_quote && !in_double_quote) {
      text[i] = '\0';
      break;
    }
  }
}

static int mini_gnb_c_case_equal(const char* left, const char* right) {
  size_t i = 0;

  if (left == NULL || right == NULL) {
    return 0;
  }

  while (left[i] != '\0' && right[i] != '\0') {
    if (tolower((unsigned char)left[i]) != tolower((unsigned char)right[i])) {
      return 0;
    }
    ++i;
  }
  return left[i] == '\0' && right[i] == '\0';
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

static int mini_gnb_c_extract_scalar(const char* text,
                                     const char* section,
                                     const char* key,
                                     char* out,
                                     size_t out_size) {
  const char* cursor = NULL;
  char current_section[32];

  if (text == NULL || section == NULL || key == NULL || out == NULL || out_size == 0U) {
    return -1;
  }

  cursor = text;
  current_section[0] = '\0';

  while (*cursor != '\0') {
    char line[256];
    size_t len = 0;
    size_t indent = 0;
    char* trimmed = NULL;
    char* colon = NULL;
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

    while (line[indent] == ' ' || line[indent] == '\t') {
      ++indent;
    }

    mini_gnb_c_strip_inline_comment(line);
    trimmed = mini_gnb_c_ltrim(line);
    mini_gnb_c_rtrim(trimmed);
    if (*trimmed == '\0') {
      continue;
    }

    colon = strchr(trimmed, ':');
    if (colon == NULL) {
      continue;
    }

    *colon = '\0';
    mini_gnb_c_rtrim(trimmed);
    value = mini_gnb_c_ltrim(colon + 1);
    mini_gnb_c_rtrim(value);

    if (indent == 0U) {
      if (*value == '\0') {
        (void)snprintf(current_section, sizeof(current_section), "%s", trimmed);
      } else {
        current_section[0] = '\0';
      }
      continue;
    }

    if (strcmp(current_section, section) != 0 || strcmp(trimmed, key) != 0) {
      continue;
    }

    mini_gnb_c_unquote(value);
    if (snprintf(out, out_size, "%s", value) >= (int)out_size) {
      return -1;
    }
    return 0;
  }

  return -1;
}

static int mini_gnb_c_extract_string(const char* text,
                                     const char* section,
                                     const char* key,
                                     char* out,
                                     const size_t out_size) {
  return mini_gnb_c_extract_scalar(text, section, key, out, out_size);
}

static int mini_gnb_c_extract_int(const char* text, const char* section, const char* key, int* out) {
  char value_text[64];
  char* end_ptr = NULL;

  if (mini_gnb_c_extract_scalar(text, section, key, value_text, sizeof(value_text)) != 0 || out == NULL) {
    return -1;
  }

  *out = (int)strtol(value_text, &end_ptr, 10);
  if (end_ptr == value_text || *mini_gnb_c_ltrim(end_ptr) != '\0') {
    return -1;
  }
  return 0;
}

static int mini_gnb_c_extract_double(const char* text,
                                     const char* section,
                                     const char* key,
                                     double* out) {
  char value_text[64];
  char* end_ptr = NULL;

  if (mini_gnb_c_extract_scalar(text, section, key, value_text, sizeof(value_text)) != 0 || out == NULL) {
    return -1;
  }

  *out = strtod(value_text, &end_ptr);
  if (end_ptr == value_text || *mini_gnb_c_ltrim(end_ptr) != '\0') {
    return -1;
  }
  return 0;
}

static int mini_gnb_c_extract_bool(const char* text, const char* section, const char* key, bool* out) {
  char value_text[16];

  if (mini_gnb_c_extract_scalar(text, section, key, value_text, sizeof(value_text)) != 0 || out == NULL) {
    return -1;
  }

  if (mini_gnb_c_case_equal(value_text, "true")) {
    *out = true;
    return 0;
  }
  if (mini_gnb_c_case_equal(value_text, "false")) {
    *out = false;
    return 0;
  }
  return -1;
}

static int mini_gnb_c_fail(char* error_message,
                           const size_t error_message_size,
                           const char* message,
                           const char* key) {
  if (error_message != NULL && error_message_size > 0U) {
    (void)snprintf(error_message, error_message_size, "%s: %s", message, key);
  }
  return -1;
}

#define MINI_GNB_C_LOAD_INT(SECTION, KEY, TARGET, CAST)                                              \
  do {                                                                                                \
    if (mini_gnb_c_extract_int(text, SECTION, KEY, &value) != 0) {                                   \
      free(text);                                                                                     \
      return mini_gnb_c_fail(error_message, error_message_size, "missing integer config key", KEY);  \
    }                                                                                                 \
    (TARGET) = (CAST)value;                                                                           \
  } while (0)

#define MINI_GNB_C_LOAD_DOUBLE(SECTION, KEY, TARGET)                                                  \
  do {                                                                                                \
    if (mini_gnb_c_extract_double(text, SECTION, KEY, &double_value) != 0) {                         \
      free(text);                                                                                     \
      return mini_gnb_c_fail(error_message, error_message_size, "missing floating config key", KEY); \
    }                                                                                                 \
    (TARGET) = double_value;                                                                          \
  } while (0)

#define MINI_GNB_C_LOAD_BOOL(SECTION, KEY, TARGET)                                                    \
  do {                                                                                                \
    if (mini_gnb_c_extract_bool(text, SECTION, KEY, &bool_value) != 0) {                             \
      free(text);                                                                                     \
      return mini_gnb_c_fail(error_message, error_message_size, "missing boolean config key", KEY);  \
    }                                                                                                 \
    (TARGET) = bool_value;                                                                            \
  } while (0)

#define MINI_GNB_C_LOAD_STRING(SECTION, KEY, TARGET)                                                  \
  do {                                                                                                \
    if (mini_gnb_c_extract_string(text, SECTION, KEY, TARGET, sizeof(TARGET)) != 0) {                \
      free(text);                                                                                     \
      return mini_gnb_c_fail(error_message, error_message_size, "missing string config key", KEY);   \
    }                                                                                                 \
  } while (0)

int mini_gnb_c_load_config(const char* path,
                           mini_gnb_c_config_t* out_config,
                           char* error_message,
                           const size_t error_message_size) {
  char* text = NULL;
  int value = 0;
  double double_value = 0.0;
  bool bool_value = false;

  if (out_config == NULL) {
    return mini_gnb_c_fail(error_message, error_message_size, "invalid output config", "out_config");
  }

  memset(out_config, 0, sizeof(*out_config));
  out_config->sim.post_msg4_traffic_enabled = false;
  out_config->sim.post_msg4_dl_data_delay_slots = 2;
  out_config->sim.post_msg4_ul_grant_delay_slots = 3;
  out_config->sim.post_msg4_ul_data_k2 = 2;
  out_config->sim.ul_data_present = false;
  out_config->sim.ul_data_crc_ok = true;
  out_config->sim.ul_data_snr_db = 16.0;
  out_config->sim.ul_data_evm = 2.5;
  out_config->sim.ul_bsr_buffer_size_bytes = 384;
  (void)snprintf(out_config->sim.ul_data_hex,
                 sizeof(out_config->sim.ul_data_hex),
                 "%s",
                 "554C5F44415441");
  text = mini_gnb_c_read_text_file(path);
  if (text == NULL) {
    return mini_gnb_c_fail(error_message, error_message_size, "failed to read config file", path);
  }

  MINI_GNB_C_LOAD_INT("cell", "dl_arfcn", out_config->cell.dl_arfcn, uint32_t);
  MINI_GNB_C_LOAD_INT("cell", "band", out_config->cell.band, uint16_t);
  MINI_GNB_C_LOAD_INT("cell", "channel_bandwidth_MHz", out_config->cell.channel_bandwidth_mhz, uint16_t);
  MINI_GNB_C_LOAD_INT("cell", "common_scs_khz", out_config->cell.common_scs_khz, uint16_t);
  MINI_GNB_C_LOAD_INT("cell", "pci", out_config->cell.pci, uint16_t);
  MINI_GNB_C_LOAD_STRING("cell", "plmn", out_config->cell.plmn);
  MINI_GNB_C_LOAD_INT("cell", "tac", out_config->cell.tac, uint16_t);
  MINI_GNB_C_LOAD_INT("cell", "ss0_index", out_config->cell.ss0_index, uint8_t);
  MINI_GNB_C_LOAD_INT("cell", "coreset0_index", out_config->cell.coreset0_index, uint8_t);

  MINI_GNB_C_LOAD_INT("prach", "prach_config_index", out_config->prach.prach_config_index, uint16_t);
  MINI_GNB_C_LOAD_INT("prach", "prach_root_seq_index", out_config->prach.prach_root_seq_index, uint16_t);
  MINI_GNB_C_LOAD_INT("prach", "zero_correlation_zone", out_config->prach.zero_correlation_zone, uint8_t);
  MINI_GNB_C_LOAD_INT("prach", "ra_resp_window", out_config->prach.ra_resp_window, uint8_t);
  MINI_GNB_C_LOAD_INT("prach", "msg3_delta_preamble", out_config->prach.msg3_delta_preamble, int8_t);

  MINI_GNB_C_LOAD_STRING("rf", "device_driver", out_config->rf.device_driver);
  MINI_GNB_C_LOAD_STRING("rf", "device_args", out_config->rf.device_args);
  MINI_GNB_C_LOAD_STRING("rf", "clock_src", out_config->rf.clock_src);
  MINI_GNB_C_LOAD_DOUBLE("rf", "srate", out_config->rf.srate);
  MINI_GNB_C_LOAD_DOUBLE("rf", "tx_gain", out_config->rf.tx_gain);
  MINI_GNB_C_LOAD_DOUBLE("rf", "rx_gain", out_config->rf.rx_gain);

  MINI_GNB_C_LOAD_INT("broadcast", "ssb_period_slots", out_config->broadcast.ssb_period_slots, int);
  MINI_GNB_C_LOAD_INT("broadcast", "sib1_period_slots", out_config->broadcast.sib1_period_slots, int);
  MINI_GNB_C_LOAD_INT("broadcast", "sib1_offset_slot", out_config->broadcast.sib1_offset_slot, int);

  MINI_GNB_C_LOAD_INT("sim", "total_slots", out_config->sim.total_slots, int);
  MINI_GNB_C_LOAD_INT("sim", "slots_per_frame", out_config->sim.slots_per_frame, int);
  MINI_GNB_C_LOAD_INT("sim", "msg3_delay_slots", out_config->sim.msg3_delay_slots, int);
  MINI_GNB_C_LOAD_INT("sim", "msg4_delay_slots", out_config->sim.msg4_delay_slots, int);
  MINI_GNB_C_LOAD_INT("sim", "prach_trigger_abs_slot", out_config->sim.prach_trigger_abs_slot, int);
  MINI_GNB_C_LOAD_INT("sim", "prach_retry_delay_slots", out_config->sim.prach_retry_delay_slots, int);
  MINI_GNB_C_LOAD_INT("sim", "preamble_id", out_config->sim.preamble_id, uint8_t);
  MINI_GNB_C_LOAD_INT("sim", "ta_est", out_config->sim.ta_est, int);
  MINI_GNB_C_LOAD_DOUBLE("sim", "peak_metric", out_config->sim.peak_metric);
  MINI_GNB_C_LOAD_BOOL("sim", "msg3_present", out_config->sim.msg3_present);
  MINI_GNB_C_LOAD_BOOL("sim", "msg3_crc_ok", out_config->sim.msg3_crc_ok);
  MINI_GNB_C_LOAD_DOUBLE("sim", "msg3_snr_db", out_config->sim.msg3_snr_db);
  MINI_GNB_C_LOAD_DOUBLE("sim", "msg3_evm", out_config->sim.msg3_evm);
  if (mini_gnb_c_extract_bool(text, "sim", "post_msg4_traffic_enabled", &bool_value) == 0) {
    out_config->sim.post_msg4_traffic_enabled = bool_value;
  }
  if (mini_gnb_c_extract_int(text, "sim", "post_msg4_dl_data_delay_slots", &value) == 0) {
    out_config->sim.post_msg4_dl_data_delay_slots = value;
  }
  if (mini_gnb_c_extract_int(text, "sim", "post_msg4_ul_grant_delay_slots", &value) == 0) {
    out_config->sim.post_msg4_ul_grant_delay_slots = value;
  }
  if (mini_gnb_c_extract_int(text, "sim", "post_msg4_ul_data_k2", &value) == 0) {
    out_config->sim.post_msg4_ul_data_k2 = value;
  }
  if (mini_gnb_c_extract_bool(text, "sim", "ul_data_present", &bool_value) == 0) {
    out_config->sim.ul_data_present = bool_value;
  }
  if (mini_gnb_c_extract_bool(text, "sim", "ul_data_crc_ok", &bool_value) == 0) {
    out_config->sim.ul_data_crc_ok = bool_value;
  }
  if (mini_gnb_c_extract_double(text, "sim", "ul_data_snr_db", &double_value) == 0) {
    out_config->sim.ul_data_snr_db = double_value;
  }
  if (mini_gnb_c_extract_double(text, "sim", "ul_data_evm", &double_value) == 0) {
    out_config->sim.ul_data_evm = double_value;
  }
  if (mini_gnb_c_extract_int(text, "sim", "ul_bsr_buffer_size_bytes", &value) == 0) {
    out_config->sim.ul_bsr_buffer_size_bytes = value;
  }
  MINI_GNB_C_LOAD_STRING("sim", "ul_prach_cf32_path", out_config->sim.ul_prach_cf32_path);
  MINI_GNB_C_LOAD_STRING("sim", "ul_msg3_cf32_path", out_config->sim.ul_msg3_cf32_path);
  if (mini_gnb_c_extract_string(text,
                                "sim",
                                "ul_input_dir",
                                out_config->sim.ul_input_dir,
                                sizeof(out_config->sim.ul_input_dir)) != 0) {
    out_config->sim.ul_input_dir[0] = '\0';
  }
  if (mini_gnb_c_extract_string(text,
                                "sim",
                                "ul_data_hex",
                                out_config->sim.ul_data_hex,
                                sizeof(out_config->sim.ul_data_hex)) != 0) {
    (void)snprintf(out_config->sim.ul_data_hex,
                   sizeof(out_config->sim.ul_data_hex),
                   "%s",
                   "554C5F44415441");
  }
  MINI_GNB_C_LOAD_STRING("sim", "contention_id_hex", out_config->sim.contention_id_hex);
  MINI_GNB_C_LOAD_INT("sim", "establishment_cause", out_config->sim.establishment_cause, uint8_t);
  MINI_GNB_C_LOAD_INT("sim", "ue_identity_type", out_config->sim.ue_identity_type, uint8_t);
  MINI_GNB_C_LOAD_STRING("sim", "ue_identity_hex", out_config->sim.ue_identity_hex);
  MINI_GNB_C_LOAD_BOOL("sim", "include_crnti_ce", out_config->sim.include_crnti_ce);

  free(text);
  if (error_message != NULL && error_message_size > 0U) {
    error_message[0] = '\0';
  }
  return 0;
}

int mini_gnb_c_format_config_summary(const mini_gnb_c_config_t* config, char* out, const size_t out_size) {
  if (config == NULL || out == NULL || out_size == 0U) {
    return -1;
  }

  if (snprintf(out,
               out_size,
               "Broadcast config summary:\n"
               "  cell pci=%u band=n%u arfcn=%u scs=%ukHz bw=%uMHz plmn=%s tac=%u\n"
               "  ss0_index=%u coreset0_index=%u ssb_period_slots=%d sib1_period_slots=%d sib1_offset_slot=%d\n"
                "RA config summary:\n"
                "  prach_config_index=%u root_seq=%u zero_corr=%u ra_resp_window=%u msg3_delta_preamble=%d\n"
                "UL input summary:\n"
                "  prach_trigger_abs_slot=%d prach_retry_delay_slots=%d msg3_delay_slots=%d msg3_present=%s input_dir=%s\n"
                "Connected traffic summary:\n"
                "  post_msg4=%s dl_delay=%d ul_grant_delay=%d ul_k2=%d ul_present=%s\n"
                "RF config summary:\n"
                "  driver=%s clock=%s srate=%g tx_gain=%g rx_gain=%g",
               config->cell.pci,
               config->cell.band,
               config->cell.dl_arfcn,
               config->cell.common_scs_khz,
               config->cell.channel_bandwidth_mhz,
               config->cell.plmn,
               config->cell.tac,
               config->cell.ss0_index,
               config->cell.coreset0_index,
               config->broadcast.ssb_period_slots,
               config->broadcast.sib1_period_slots,
               config->broadcast.sib1_offset_slot,
               config->prach.prach_config_index,
               config->prach.prach_root_seq_index,
               config->prach.zero_correlation_zone,
               config->prach.ra_resp_window,
               config->prach.msg3_delta_preamble,
                config->sim.prach_trigger_abs_slot,
                config->sim.prach_retry_delay_slots,
                config->sim.msg3_delay_slots,
                config->sim.msg3_present ? "true" : "false",
                config->sim.ul_input_dir[0] != '\0' ? config->sim.ul_input_dir : "(disabled)",
                config->sim.post_msg4_traffic_enabled ? "true" : "false",
                config->sim.post_msg4_dl_data_delay_slots,
                config->sim.post_msg4_ul_grant_delay_slots,
                config->sim.post_msg4_ul_data_k2,
                config->sim.ul_data_present ? "true" : "false",
                config->rf.device_driver,
                config->rf.clock_src,
                config->rf.srate,
               config->rf.tx_gain,
               config->rf.rx_gain) >= (int)out_size) {
    return -1;
  }

  return 0;
}
