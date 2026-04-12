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

static int mini_gnb_c_parse_rf_runtime_mode_text(const char* text, mini_gnb_c_rf_runtime_mode_t* out_mode) {
  if (text == NULL || out_mode == NULL) {
    return -1;
  }
  if (mini_gnb_c_case_equal(text, "simulator") || mini_gnb_c_case_equal(text, "sim")) {
    *out_mode = MINI_GNB_C_RF_RUNTIME_MODE_SIMULATOR;
    return 0;
  }
  if (mini_gnb_c_case_equal(text, "rx")) {
    *out_mode = MINI_GNB_C_RF_RUNTIME_MODE_RX;
    return 0;
  }
  if (mini_gnb_c_case_equal(text, "tx")) {
    *out_mode = MINI_GNB_C_RF_RUNTIME_MODE_TX;
    return 0;
  }
  if (mini_gnb_c_case_equal(text, "trx")) {
    *out_mode = MINI_GNB_C_RF_RUNTIME_MODE_TRX;
    return 0;
  }
  return -1;
}

static int mini_gnb_c_parse_rf_duration_mode_text(const char* text, mini_gnb_c_rf_duration_mode_t* out_mode) {
  if (text == NULL || out_mode == NULL) {
    return -1;
  }
  if (mini_gnb_c_case_equal(text, "samples") || mini_gnb_c_case_equal(text, "sample_target")) {
    *out_mode = MINI_GNB_C_RF_DURATION_MODE_SAMPLES;
    return 0;
  }
  if (mini_gnb_c_case_equal(text, "wallclock")) {
    *out_mode = MINI_GNB_C_RF_DURATION_MODE_WALLCLOCK;
    return 0;
  }
  return -1;
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
  char scalar_text[64];

  if (out_config == NULL) {
    return mini_gnb_c_fail(error_message, error_message_size, "invalid output config", "out_config");
  }

  memset(out_config, 0, sizeof(*out_config));
  out_config->sim.post_msg4_traffic_enabled = false;
  out_config->core.enabled = false;
  (void)snprintf(out_config->core.amf_ip, sizeof(out_config->core.amf_ip), "%s", "127.0.0.5");
  out_config->core.amf_port = 38412u;
  out_config->core.upf_port = 2152u;
  out_config->core.timeout_ms = 5000u;
  out_config->core.ran_ue_ngap_id_base = 1u;
  out_config->core.default_pdu_session_id = 1u;
  out_config->core.ngap_trace_pcap[0] = '\0';
  out_config->core.gtpu_trace_pcap[0] = '\0';
  out_config->rf.subdev[0] = '\0';
  out_config->rf.runtime_mode = MINI_GNB_C_RF_RUNTIME_MODE_SIMULATOR;
  out_config->rf.freq_hz = 2462e6;
  out_config->rf.rx_freq_hz = out_config->rf.freq_hz;
  out_config->rf.tx_freq_hz = out_config->rf.freq_hz;
  out_config->rf.bandwidth_hz = 20e6;
  out_config->rf.duration_sec = 1.0;
  out_config->rf.duration_mode = MINI_GNB_C_RF_DURATION_MODE_SAMPLES;
  out_config->rf.channel = 0u;
  out_config->rf.channel_count = 1u;
  out_config->rf.rx_cpu_core = -1;
  out_config->rf.tx_cpu_core = -1;
  out_config->rf.apply_host_tuning = true;
  out_config->rf.require_ref_lock = true;
  out_config->rf.require_lo_lock = true;
  out_config->rf.rx_output_file[0] = '\0';
  out_config->rf.tx_input_file[0] = '\0';
  out_config->rf.rx_ring_map[0] = '\0';
  out_config->rf.tx_ring_map[0] = '\0';
  out_config->rf.ring_block_samples = 4096u;
  out_config->rf.ring_block_count = 1024u;
  out_config->rf.tx_prefetch_samples = 0u;
  out_config->real_cell.enabled = false;
  out_config->real_cell.profile_name[0] = '\0';
  out_config->real_cell.target_backend[0] = '\0';
  out_config->sim.post_msg4_dl_pdcch_delay_slots = 1;
  out_config->sim.post_msg4_dl_time_indicator = 1;
  out_config->sim.post_msg4_dl_data_to_ul_ack_slots = 1;
  out_config->sim.post_msg4_sr_period_slots = 10;
  out_config->sim.post_msg4_sr_offset_slot = 2;
  out_config->sim.post_msg4_ul_grant_delay_slots = 1;
  out_config->sim.post_msg4_ul_time_indicator = 2;
  out_config->sim.post_msg4_dl_harq_process_count = 2;
  out_config->sim.post_msg4_ul_harq_process_count = 2;
  out_config->sim.ul_data_present = false;
  out_config->sim.ul_data_crc_ok = true;
  out_config->sim.ul_data_snr_db = 16.0;
  out_config->sim.ul_data_evm = 2.5;
  out_config->sim.ul_bsr_buffer_size_bytes = 384;
  out_config->sim.slot_sleep_ms = 0u;
  out_config->sim.local_exchange_dir[0] = '\0';
  out_config->sim.shared_slot_path[0] = '\0';
  out_config->sim.shared_slot_timeout_ms = 100u;
  out_config->sim.ue_tun_enabled = false;
  (void)snprintf(out_config->sim.ue_tun_name, sizeof(out_config->sim.ue_tun_name), "%s", "miniue0");
  out_config->sim.ue_tun_mtu = 1400u;
  out_config->sim.ue_tun_prefix_len = 16u;
  out_config->sim.ue_tun_isolate_netns = true;
  out_config->sim.ue_tun_add_default_route = true;
  out_config->sim.ue_tun_netns_name[0] = '\0';
  out_config->sim.ue_tun_dns_server_ipv4[0] = '\0';
  out_config->sim.scripted_schedule_dir[0] = '\0';
  out_config->sim.scripted_pdcch_dir[0] = '\0';
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
  if (mini_gnb_c_extract_string(text, "rf", "runtime_mode", scalar_text, sizeof(scalar_text)) == 0 &&
      mini_gnb_c_parse_rf_runtime_mode_text(scalar_text, &out_config->rf.runtime_mode) != 0) {
    free(text);
    return mini_gnb_c_fail(error_message, error_message_size, "invalid rf.runtime_mode", path);
  }
  MINI_GNB_C_LOAD_DOUBLE("rf", "srate", out_config->rf.srate);
  if (mini_gnb_c_extract_double(text, "rf", "freq_hz", &double_value) == 0) {
    out_config->rf.freq_hz = double_value;
    out_config->rf.rx_freq_hz = double_value;
    out_config->rf.tx_freq_hz = double_value;
  }
  if (mini_gnb_c_extract_double(text, "rf", "rx_freq_hz", &double_value) == 0) {
    out_config->rf.rx_freq_hz = double_value;
  }
  if (mini_gnb_c_extract_double(text, "rf", "tx_freq_hz", &double_value) == 0) {
    out_config->rf.tx_freq_hz = double_value;
  }
  MINI_GNB_C_LOAD_DOUBLE("rf", "tx_gain", out_config->rf.tx_gain);
  MINI_GNB_C_LOAD_DOUBLE("rf", "rx_gain", out_config->rf.rx_gain);
  if (mini_gnb_c_extract_double(text, "rf", "bandwidth_hz", &double_value) == 0) {
    out_config->rf.bandwidth_hz = double_value;
  }
  if (mini_gnb_c_extract_double(text, "rf", "duration_sec", &double_value) == 0) {
    out_config->rf.duration_sec = double_value;
  }
  if (mini_gnb_c_extract_string(text, "rf", "duration_mode", scalar_text, sizeof(scalar_text)) == 0 &&
      mini_gnb_c_parse_rf_duration_mode_text(scalar_text, &out_config->rf.duration_mode) != 0) {
    free(text);
    return mini_gnb_c_fail(error_message, error_message_size, "invalid rf.duration_mode", path);
  }
  if (mini_gnb_c_extract_string(text, "rf", "subdev", out_config->rf.subdev, sizeof(out_config->rf.subdev)) != 0) {
    out_config->rf.subdev[0] = '\0';
  }
  if (mini_gnb_c_extract_int(text, "rf", "channel", &value) == 0 && value >= 0) {
    out_config->rf.channel = (uint32_t)value;
  }
  if (mini_gnb_c_extract_int(text, "rf", "channel_count", &value) == 0 && value > 0) {
    out_config->rf.channel_count = (uint32_t)value;
  }
  if (mini_gnb_c_extract_int(text, "rf", "rx_cpu_core", &value) == 0) {
    out_config->rf.rx_cpu_core = value;
  }
  if (mini_gnb_c_extract_int(text, "rf", "tx_cpu_core", &value) == 0) {
    out_config->rf.tx_cpu_core = value;
  }
  if (mini_gnb_c_extract_int(text, "rf", "tx_prefetch_samples", &value) == 0 && value > 0) {
    out_config->rf.tx_prefetch_samples = (uint32_t)value;
  }
  if (mini_gnb_c_extract_int(text, "rf", "ring_block_samples", &value) == 0 && value > 0) {
    out_config->rf.ring_block_samples = (uint32_t)value;
  }
  if (mini_gnb_c_extract_int(text, "rf", "ring_block_count", &value) == 0 && value > 0) {
    out_config->rf.ring_block_count = (uint32_t)value;
  }
  if (mini_gnb_c_extract_bool(text, "rf", "apply_host_tuning", &bool_value) == 0) {
    out_config->rf.apply_host_tuning = bool_value;
  }
  if (mini_gnb_c_extract_bool(text, "rf", "require_ref_lock", &bool_value) == 0) {
    out_config->rf.require_ref_lock = bool_value;
  }
  if (mini_gnb_c_extract_bool(text, "rf", "require_lo_lock", &bool_value) == 0) {
    out_config->rf.require_lo_lock = bool_value;
  }
  if (mini_gnb_c_extract_string(text,
                                "rf",
                                "rx_output_file",
                                out_config->rf.rx_output_file,
                                sizeof(out_config->rf.rx_output_file)) != 0) {
    out_config->rf.rx_output_file[0] = '\0';
  }
  if (mini_gnb_c_extract_string(text,
                                "rf",
                                "tx_input_file",
                                out_config->rf.tx_input_file,
                                sizeof(out_config->rf.tx_input_file)) != 0) {
    out_config->rf.tx_input_file[0] = '\0';
  }
  if (mini_gnb_c_extract_string(text,
                                "rf",
                                "rx_ring_map",
                                out_config->rf.rx_ring_map,
                                sizeof(out_config->rf.rx_ring_map)) != 0) {
    out_config->rf.rx_ring_map[0] = '\0';
  }
  if (mini_gnb_c_extract_string(text,
                                "rf",
                                "tx_ring_map",
                                out_config->rf.tx_ring_map,
                                sizeof(out_config->rf.tx_ring_map)) != 0) {
    out_config->rf.tx_ring_map[0] = '\0';
  }

  (void)snprintf(out_config->real_cell.profile_name,
                 sizeof(out_config->real_cell.profile_name),
                 "%s",
                 "b210_n78_demo");
  (void)snprintf(out_config->real_cell.target_backend,
                 sizeof(out_config->real_cell.target_backend),
                 "%s",
                 "uhd-b210");
  out_config->real_cell.dl_arfcn = out_config->cell.dl_arfcn;
  out_config->real_cell.band = out_config->cell.band;
  out_config->real_cell.channel_bandwidth_mhz = out_config->cell.channel_bandwidth_mhz;
  out_config->real_cell.common_scs_khz = out_config->cell.common_scs_khz;
  (void)snprintf(out_config->real_cell.plmn, sizeof(out_config->real_cell.plmn), "%s", out_config->cell.plmn);
  out_config->real_cell.tac = out_config->cell.tac;

  if (mini_gnb_c_extract_bool(text, "real_cell", "enabled", &bool_value) == 0) {
    out_config->real_cell.enabled = bool_value;
  }
  if (mini_gnb_c_extract_string(text,
                                "real_cell",
                                "profile_name",
                                out_config->real_cell.profile_name,
                                sizeof(out_config->real_cell.profile_name)) != 0) {
    (void)snprintf(out_config->real_cell.profile_name,
                   sizeof(out_config->real_cell.profile_name),
                   "%s",
                   "b210_n78_demo");
  }
  if (mini_gnb_c_extract_string(text,
                                "real_cell",
                                "target_backend",
                                out_config->real_cell.target_backend,
                                sizeof(out_config->real_cell.target_backend)) != 0) {
    (void)snprintf(out_config->real_cell.target_backend,
                   sizeof(out_config->real_cell.target_backend),
                   "%s",
                   "uhd-b210");
  }
  if (mini_gnb_c_extract_int(text, "real_cell", "dl_arfcn", &value) == 0) {
    out_config->real_cell.dl_arfcn = (uint32_t)value;
  }
  if (mini_gnb_c_extract_int(text, "real_cell", "band", &value) == 0) {
    out_config->real_cell.band = (uint16_t)value;
  }
  if (mini_gnb_c_extract_int(text, "real_cell", "channel_bandwidth_MHz", &value) == 0) {
    out_config->real_cell.channel_bandwidth_mhz = (uint16_t)value;
  }
  if (mini_gnb_c_extract_int(text, "real_cell", "common_scs_khz", &value) == 0) {
    out_config->real_cell.common_scs_khz = (uint16_t)value;
  }
  if (mini_gnb_c_extract_string(text,
                                "real_cell",
                                "plmn",
                                out_config->real_cell.plmn,
                                sizeof(out_config->real_cell.plmn)) != 0) {
    (void)snprintf(out_config->real_cell.plmn, sizeof(out_config->real_cell.plmn), "%s", out_config->cell.plmn);
  }
  if (mini_gnb_c_extract_int(text, "real_cell", "tac", &value) == 0) {
    out_config->real_cell.tac = (uint16_t)value;
  }

  if (mini_gnb_c_extract_bool(text, "core", "enabled", &bool_value) == 0) {
    out_config->core.enabled = bool_value;
  }
  if (mini_gnb_c_extract_string(text, "core", "amf_ip", out_config->core.amf_ip, sizeof(out_config->core.amf_ip)) !=
      0) {
    (void)snprintf(out_config->core.amf_ip, sizeof(out_config->core.amf_ip), "%s", "127.0.0.5");
  }
  if (mini_gnb_c_extract_int(text, "core", "amf_port", &value) == 0) {
    out_config->core.amf_port = (uint32_t)value;
  }
  if (mini_gnb_c_extract_int(text, "core", "upf_port", &value) == 0) {
    out_config->core.upf_port = (uint16_t)value;
  }
  if (mini_gnb_c_extract_int(text, "core", "timeout_ms", &value) == 0) {
    out_config->core.timeout_ms = (uint32_t)value;
  }
  if (mini_gnb_c_extract_int(text, "core", "ran_ue_ngap_id_base", &value) == 0) {
    out_config->core.ran_ue_ngap_id_base = (uint16_t)value;
  }
  if (mini_gnb_c_extract_int(text, "core", "default_pdu_session_id", &value) == 0) {
    out_config->core.default_pdu_session_id = (uint8_t)value;
  }
  if (mini_gnb_c_extract_string(text,
                                "core",
                                "ngap_trace_pcap",
                                out_config->core.ngap_trace_pcap,
                                sizeof(out_config->core.ngap_trace_pcap)) != 0) {
    out_config->core.ngap_trace_pcap[0] = '\0';
  }
  if (mini_gnb_c_extract_string(text,
                                "core",
                                "gtpu_trace_pcap",
                                out_config->core.gtpu_trace_pcap,
                                sizeof(out_config->core.gtpu_trace_pcap)) != 0) {
    out_config->core.gtpu_trace_pcap[0] = '\0';
  }

  MINI_GNB_C_LOAD_INT("broadcast", "ssb_period_slots", out_config->broadcast.ssb_period_slots, int);
  MINI_GNB_C_LOAD_INT("broadcast", "sib1_period_slots", out_config->broadcast.sib1_period_slots, int);
  MINI_GNB_C_LOAD_INT("broadcast", "sib1_offset_slot", out_config->broadcast.sib1_offset_slot, int);
  MINI_GNB_C_LOAD_INT("broadcast", "prach_period_slots", out_config->broadcast.prach_period_slots, int);
  MINI_GNB_C_LOAD_INT("broadcast", "prach_offset_slot", out_config->broadcast.prach_offset_slot, int);

  MINI_GNB_C_LOAD_INT("sim", "total_slots", out_config->sim.total_slots, int);
  MINI_GNB_C_LOAD_INT("sim", "slots_per_frame", out_config->sim.slots_per_frame, int);
  if (mini_gnb_c_extract_int(text, "sim", "slot_sleep_ms", &value) == 0) {
    out_config->sim.slot_sleep_ms = (uint32_t)value;
  }
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
  if (mini_gnb_c_extract_int(text, "sim", "post_msg4_dl_pdcch_delay_slots", &value) == 0) {
    out_config->sim.post_msg4_dl_pdcch_delay_slots = value;
  } else if (mini_gnb_c_extract_int(text, "sim", "post_msg4_dl_data_delay_slots", &value) == 0) {
    out_config->sim.post_msg4_dl_pdcch_delay_slots = value;
  }
  if (mini_gnb_c_extract_int(text, "sim", "post_msg4_dl_time_indicator", &value) == 0) {
    out_config->sim.post_msg4_dl_time_indicator = value;
  }
  if (mini_gnb_c_extract_int(text, "sim", "post_msg4_dl_data_to_ul_ack_slots", &value) == 0) {
    out_config->sim.post_msg4_dl_data_to_ul_ack_slots = value;
  }
  if (mini_gnb_c_extract_int(text, "sim", "post_msg4_sr_period_slots", &value) == 0) {
    out_config->sim.post_msg4_sr_period_slots = value;
  }
  if (mini_gnb_c_extract_int(text, "sim", "post_msg4_sr_offset_slot", &value) == 0) {
    out_config->sim.post_msg4_sr_offset_slot = value;
  }
  if (mini_gnb_c_extract_int(text, "sim", "post_msg4_ul_grant_delay_slots", &value) == 0) {
    out_config->sim.post_msg4_ul_grant_delay_slots = value;
  }
  if (mini_gnb_c_extract_int(text, "sim", "post_msg4_ul_time_indicator", &value) == 0) {
    out_config->sim.post_msg4_ul_time_indicator = value;
  } else if (mini_gnb_c_extract_int(text, "sim", "post_msg4_ul_data_k2", &value) == 0) {
    out_config->sim.post_msg4_ul_time_indicator = value;
  }
  if (mini_gnb_c_extract_int(text, "sim", "post_msg4_dl_harq_process_count", &value) == 0) {
    out_config->sim.post_msg4_dl_harq_process_count = value;
  }
  if (mini_gnb_c_extract_int(text, "sim", "post_msg4_ul_harq_process_count", &value) == 0) {
    out_config->sim.post_msg4_ul_harq_process_count = value;
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
                                "local_exchange_dir",
                                out_config->sim.local_exchange_dir,
                                sizeof(out_config->sim.local_exchange_dir)) != 0) {
    out_config->sim.local_exchange_dir[0] = '\0';
  }
  if (mini_gnb_c_extract_string(text,
                                "sim",
                                "shared_slot_path",
                                out_config->sim.shared_slot_path,
                                sizeof(out_config->sim.shared_slot_path)) != 0) {
    out_config->sim.shared_slot_path[0] = '\0';
  }
  if (mini_gnb_c_extract_int(text, "sim", "shared_slot_timeout_ms", &value) == 0) {
    out_config->sim.shared_slot_timeout_ms = (uint32_t)value;
  }
  if (mini_gnb_c_extract_bool(text, "sim", "ue_tun_enabled", &bool_value) == 0) {
    out_config->sim.ue_tun_enabled = bool_value;
  }
  if (mini_gnb_c_extract_string(text,
                                "sim",
                                "ue_tun_name",
                                out_config->sim.ue_tun_name,
                                sizeof(out_config->sim.ue_tun_name)) != 0) {
    (void)snprintf(out_config->sim.ue_tun_name, sizeof(out_config->sim.ue_tun_name), "%s", "miniue0");
  }
  if (mini_gnb_c_extract_int(text, "sim", "ue_tun_mtu", &value) == 0) {
    out_config->sim.ue_tun_mtu = (uint16_t)value;
  }
  if (mini_gnb_c_extract_int(text, "sim", "ue_tun_prefix_len", &value) == 0) {
    out_config->sim.ue_tun_prefix_len = (uint8_t)value;
  }
  if (mini_gnb_c_extract_bool(text, "sim", "ue_tun_isolate_netns", &bool_value) == 0) {
    out_config->sim.ue_tun_isolate_netns = bool_value;
  }
  if (mini_gnb_c_extract_bool(text, "sim", "ue_tun_add_default_route", &bool_value) == 0) {
    out_config->sim.ue_tun_add_default_route = bool_value;
  }
  if (mini_gnb_c_extract_string(text,
                                "sim",
                                "ue_tun_netns_name",
                                out_config->sim.ue_tun_netns_name,
                                sizeof(out_config->sim.ue_tun_netns_name)) != 0) {
    out_config->sim.ue_tun_netns_name[0] = '\0';
  }
  if (mini_gnb_c_extract_string(text,
                                "sim",
                                "ue_tun_dns_server_ipv4",
                                out_config->sim.ue_tun_dns_server_ipv4,
                                sizeof(out_config->sim.ue_tun_dns_server_ipv4)) != 0) {
    out_config->sim.ue_tun_dns_server_ipv4[0] = '\0';
  }
  if (mini_gnb_c_extract_string(text,
                                "sim",
                                "scripted_schedule_dir",
                                out_config->sim.scripted_schedule_dir,
                                sizeof(out_config->sim.scripted_schedule_dir)) != 0) {
    out_config->sim.scripted_schedule_dir[0] = '\0';
  }
  if (mini_gnb_c_extract_string(text,
                                "sim",
                                "scripted_pdcch_dir",
                                out_config->sim.scripted_pdcch_dir,
                                sizeof(out_config->sim.scripted_pdcch_dir)) != 0) {
    out_config->sim.scripted_pdcch_dir[0] = '\0';
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
               "  ss0_index=%u coreset0_index=%u ssb_period_slots=%d sib1_period_slots=%d sib1_offset_slot=%d prach_period_slots=%d prach_offset_slot=%d\n"
                "RA config summary:\n"
                "  prach_config_index=%u root_seq=%u zero_corr=%u ra_resp_window=%u msg3_delta_preamble=%d\n"
                "UL input summary:\n"
                "  prach_trigger_abs_slot=%d prach_retry_delay_slots=%d msg3_delay_slots=%d msg3_present=%s input_dir=%s local_exchange_dir=%s shared_slot_path=%s shared_slot_timeout_ms=%u slot_sleep_ms=%u scripted_schedule_dir=%s scripted_pdcch_dir=%s\n"
                "UE runtime summary:\n"
                "  ue_tun_enabled=%s ue_tun_name=%s ue_tun_mtu=%u ue_tun_prefix_len=%u ue_tun_isolate_netns=%s ue_tun_add_default_route=%s ue_tun_netns_name=%s ue_tun_dns_server_ipv4=%s\n"
                "Connected traffic summary:\n"
                "  post_msg4=%s dl_pdcch_delay=%d dl_time_indicator=%d dl_ack=%d sr_period=%d sr_offset=%d ul_grant_delay=%d ul_time_indicator=%d dl_harq=%d ul_harq=%d ul_present=%s\n"
               "Core bridge summary:\n"
                "  enabled=%s amf=%s:%u upf_port=%u timeout_ms=%u ran_ue_ngap_id_base=%u default_pdu_session_id=%u ngap_trace_pcap=%s gtpu_trace_pcap=%s\n"
                "RF config summary:\n"
                "  driver=%s runtime_mode=%s subdev=%s clock=%s srate=%g freq_hz=%g rx_freq_hz=%g tx_freq_hz=%g tx_gain=%g rx_gain=%g bandwidth_hz=%g duration_sec=%g duration_mode=%s channel=%u channel_count=%u rx_cpu_core=%d tx_cpu_core=%d apply_host_tuning=%s require_ref_lock=%s require_lo_lock=%s tx_prefetch_samples=%u ring_block_samples=%u ring_block_count=%u rx_output_file=%s tx_input_file=%s rx_ring_map=%s tx_ring_map=%s\n"
                "Stage 1 real-cell summary:\n"
                "  enabled=%s profile=%s target_backend=%s band=n%u arfcn=%u scs=%ukHz bw=%uMHz plmn=%s tac=%u",
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
               config->broadcast.prach_period_slots,
               config->broadcast.prach_offset_slot,
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
                config->sim.local_exchange_dir[0] != '\0' ? config->sim.local_exchange_dir : "(disabled)",
                config->sim.shared_slot_path[0] != '\0' ? config->sim.shared_slot_path : "(disabled)",
                config->sim.shared_slot_timeout_ms,
                config->sim.slot_sleep_ms,
                config->sim.scripted_schedule_dir[0] != '\0' ? config->sim.scripted_schedule_dir : "(disabled)",
                config->sim.scripted_pdcch_dir[0] != '\0' ? config->sim.scripted_pdcch_dir : "(disabled)",
                config->sim.ue_tun_enabled ? "true" : "false",
                config->sim.ue_tun_name,
                config->sim.ue_tun_mtu,
                config->sim.ue_tun_prefix_len,
                config->sim.ue_tun_isolate_netns ? "true" : "false",
                config->sim.ue_tun_add_default_route ? "true" : "false",
                config->sim.ue_tun_netns_name[0] != '\0' ? config->sim.ue_tun_netns_name : "(disabled)",
                config->sim.ue_tun_dns_server_ipv4[0] != '\0' ? config->sim.ue_tun_dns_server_ipv4 : "(disabled)",
                config->sim.post_msg4_traffic_enabled ? "true" : "false",
                config->sim.post_msg4_dl_pdcch_delay_slots,
                config->sim.post_msg4_dl_time_indicator,
                config->sim.post_msg4_dl_data_to_ul_ack_slots,
                config->sim.post_msg4_sr_period_slots,
                config->sim.post_msg4_sr_offset_slot,
                config->sim.post_msg4_ul_grant_delay_slots,
                config->sim.post_msg4_ul_time_indicator,
                config->sim.post_msg4_dl_harq_process_count,
                config->sim.post_msg4_ul_harq_process_count,
                config->sim.ul_data_present ? "true" : "false",
                config->core.enabled ? "true" : "false",
                config->core.amf_ip,
                config->core.amf_port,
                config->core.upf_port,
                config->core.timeout_ms,
                config->core.ran_ue_ngap_id_base,
                config->core.default_pdu_session_id,
                config->core.ngap_trace_pcap[0] != '\0' ? config->core.ngap_trace_pcap : "(auto)",
                config->core.gtpu_trace_pcap[0] != '\0' ? config->core.gtpu_trace_pcap : "(auto)",
                config->rf.device_driver,
                mini_gnb_c_rf_runtime_mode_to_string(config->rf.runtime_mode),
                config->rf.subdev[0] != '\0' ? config->rf.subdev : "(default)",
                config->rf.clock_src,
                config->rf.srate,
                config->rf.freq_hz,
                config->rf.rx_freq_hz,
                config->rf.tx_freq_hz,
                config->rf.tx_gain,
                config->rf.rx_gain,
                config->rf.bandwidth_hz,
                config->rf.duration_sec,
                mini_gnb_c_rf_duration_mode_to_string(config->rf.duration_mode),
                config->rf.channel,
                config->rf.channel_count,
                config->rf.rx_cpu_core,
                config->rf.tx_cpu_core,
                config->rf.apply_host_tuning ? "true" : "false",
                config->rf.require_ref_lock ? "true" : "false",
                config->rf.require_lo_lock ? "true" : "false",
                config->rf.tx_prefetch_samples,
                config->rf.ring_block_samples,
                config->rf.ring_block_count,
                config->rf.rx_output_file[0] != '\0' ? config->rf.rx_output_file : "(auto)",
                config->rf.tx_input_file[0] != '\0' ? config->rf.tx_input_file : "(disabled)",
                config->rf.rx_ring_map[0] != '\0' ? config->rf.rx_ring_map : "(disabled)",
                config->rf.tx_ring_map[0] != '\0' ? config->rf.tx_ring_map : "(disabled)",
                config->real_cell.enabled ? "true" : "false",
                config->real_cell.profile_name,
                config->real_cell.target_backend,
                config->real_cell.band,
                config->real_cell.dl_arfcn,
                config->real_cell.common_scs_khz,
                config->real_cell.channel_bandwidth_mhz,
                config->real_cell.plmn,
                config->real_cell.tac) >= (int)out_size) {
    return -1;
  }

  return 0;
}
