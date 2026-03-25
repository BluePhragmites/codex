#include "mini_gnb_c/config/config_loader.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mini_gnb_c/common/json_utils.h"

static const char* mini_gnb_c_find_key(const char* text, const char* key) {
  static char pattern[64];
  if (snprintf(pattern, sizeof(pattern), "\"%s\"", key) >= (int)sizeof(pattern)) {
    return NULL;
  }
  return strstr(text, pattern);
}

static const char* mini_gnb_c_find_value_start(const char* text, const char* key) {
  const char* cursor = mini_gnb_c_find_key(text, key);
  if (cursor == NULL) {
    return NULL;
  }
  cursor = strchr(cursor, ':');
  if (cursor == NULL) {
    return NULL;
  }
  ++cursor;
  while (*cursor != '\0' && isspace((unsigned char)*cursor)) {
    ++cursor;
  }
  return cursor;
}

static int mini_gnb_c_extract_string(const char* text,
                                     const char* key,
                                     char* out,
                                     const size_t out_size) {
  const char* start = mini_gnb_c_find_value_start(text, key);
  const char* end = NULL;
  size_t len = 0;

  if (start == NULL || *start != '"' || out == NULL || out_size == 0U) {
    return -1;
  }
  ++start;
  end = strchr(start, '"');
  if (end == NULL) {
    return -1;
  }
  len = (size_t)(end - start);
  if (len + 1U > out_size) {
    return -1;
  }
  memcpy(out, start, len);
  out[len] = '\0';
  return 0;
}

static int mini_gnb_c_extract_int(const char* text, const char* key, int* out) {
  char* end_ptr = NULL;
  const char* start = mini_gnb_c_find_value_start(text, key);
  if (start == NULL || out == NULL) {
    return -1;
  }
  *out = (int)strtol(start, &end_ptr, 10);
  if (end_ptr == start) {
    return -1;
  }
  return 0;
}

static int mini_gnb_c_extract_double(const char* text, const char* key, double* out) {
  char* end_ptr = NULL;
  const char* start = mini_gnb_c_find_value_start(text, key);
  if (start == NULL || out == NULL) {
    return -1;
  }
  *out = strtod(start, &end_ptr);
  if (end_ptr == start) {
    return -1;
  }
  return 0;
}

static int mini_gnb_c_extract_bool(const char* text, const char* key, bool* out) {
  const char* start = mini_gnb_c_find_value_start(text, key);
  if (start == NULL || out == NULL) {
    return -1;
  }
  if (strncmp(start, "true", 4U) == 0) {
    *out = true;
    return 0;
  }
  if (strncmp(start, "false", 5U) == 0) {
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
  text = mini_gnb_c_read_text_file(path);
  if (text == NULL) {
    return mini_gnb_c_fail(error_message, error_message_size, "failed to read config file", path);
  }

  if (mini_gnb_c_extract_int(text, "dl_arfcn", &value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing integer config key", "dl_arfcn"); }
  out_config->cell.dl_arfcn = (uint32_t)value;
  if (mini_gnb_c_extract_int(text, "band", &value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing integer config key", "band"); }
  out_config->cell.band = (uint16_t)value;
  if (mini_gnb_c_extract_int(text, "channel_bandwidth_MHz", &value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing integer config key", "channel_bandwidth_MHz"); }
  out_config->cell.channel_bandwidth_mhz = (uint16_t)value;
  if (mini_gnb_c_extract_int(text, "common_scs_khz", &value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing integer config key", "common_scs_khz"); }
  out_config->cell.common_scs_khz = (uint16_t)value;
  if (mini_gnb_c_extract_int(text, "pci", &value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing integer config key", "pci"); }
  out_config->cell.pci = (uint16_t)value;
  if (mini_gnb_c_extract_string(text, "plmn", out_config->cell.plmn, sizeof(out_config->cell.plmn)) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing string config key", "plmn"); }
  if (mini_gnb_c_extract_int(text, "tac", &value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing integer config key", "tac"); }
  out_config->cell.tac = (uint16_t)value;
  if (mini_gnb_c_extract_int(text, "ss0_index", &value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing integer config key", "ss0_index"); }
  out_config->cell.ss0_index = (uint8_t)value;
  if (mini_gnb_c_extract_int(text, "coreset0_index", &value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing integer config key", "coreset0_index"); }
  out_config->cell.coreset0_index = (uint8_t)value;

  if (mini_gnb_c_extract_int(text, "prach_config_index", &value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing integer config key", "prach_config_index"); }
  out_config->prach.prach_config_index = (uint16_t)value;
  if (mini_gnb_c_extract_int(text, "prach_root_seq_index", &value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing integer config key", "prach_root_seq_index"); }
  out_config->prach.prach_root_seq_index = (uint16_t)value;
  if (mini_gnb_c_extract_int(text, "zero_correlation_zone", &value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing integer config key", "zero_correlation_zone"); }
  out_config->prach.zero_correlation_zone = (uint8_t)value;
  if (mini_gnb_c_extract_int(text, "ra_resp_window", &value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing integer config key", "ra_resp_window"); }
  out_config->prach.ra_resp_window = (uint8_t)value;
  if (mini_gnb_c_extract_int(text, "msg3_delta_preamble", &value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing integer config key", "msg3_delta_preamble"); }
  out_config->prach.msg3_delta_preamble = (int8_t)value;

  if (mini_gnb_c_extract_string(text, "device_driver", out_config->rf.device_driver, sizeof(out_config->rf.device_driver)) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing string config key", "device_driver"); }
  if (mini_gnb_c_extract_string(text, "device_args", out_config->rf.device_args, sizeof(out_config->rf.device_args)) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing string config key", "device_args"); }
  if (mini_gnb_c_extract_string(text, "clock_src", out_config->rf.clock_src, sizeof(out_config->rf.clock_src)) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing string config key", "clock_src"); }
  if (mini_gnb_c_extract_double(text, "srate", &double_value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing floating config key", "srate"); }
  out_config->rf.srate = double_value;
  if (mini_gnb_c_extract_double(text, "tx_gain", &double_value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing floating config key", "tx_gain"); }
  out_config->rf.tx_gain = double_value;
  if (mini_gnb_c_extract_double(text, "rx_gain", &double_value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing floating config key", "rx_gain"); }
  out_config->rf.rx_gain = double_value;

  if (mini_gnb_c_extract_int(text, "ssb_period_slots", &value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing integer config key", "ssb_period_slots"); }
  out_config->broadcast.ssb_period_slots = value;
  if (mini_gnb_c_extract_int(text, "sib1_period_slots", &value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing integer config key", "sib1_period_slots"); }
  out_config->broadcast.sib1_period_slots = value;

  if (mini_gnb_c_extract_int(text, "total_slots", &value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing integer config key", "total_slots"); }
  out_config->sim.total_slots = value;
  if (mini_gnb_c_extract_int(text, "slots_per_frame", &value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing integer config key", "slots_per_frame"); }
  out_config->sim.slots_per_frame = value;
  if (mini_gnb_c_extract_int(text, "msg3_delay_slots", &value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing integer config key", "msg3_delay_slots"); }
  out_config->sim.msg3_delay_slots = value;
  if (mini_gnb_c_extract_int(text, "msg4_delay_slots", &value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing integer config key", "msg4_delay_slots"); }
  out_config->sim.msg4_delay_slots = value;
  if (mini_gnb_c_extract_int(text, "prach_trigger_abs_slot", &value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing integer config key", "prach_trigger_abs_slot"); }
  out_config->sim.prach_trigger_abs_slot = value;
  if (mini_gnb_c_extract_int(text, "preamble_id", &value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing integer config key", "preamble_id"); }
  out_config->sim.preamble_id = (uint8_t)value;
  if (mini_gnb_c_extract_int(text, "ta_est", &value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing integer config key", "ta_est"); }
  out_config->sim.ta_est = value;
  if (mini_gnb_c_extract_double(text, "peak_metric", &double_value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing floating config key", "peak_metric"); }
  out_config->sim.peak_metric = double_value;
  if (mini_gnb_c_extract_bool(text, "msg3_crc_ok", &bool_value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing boolean config key", "msg3_crc_ok"); }
  out_config->sim.msg3_crc_ok = bool_value;
  if (mini_gnb_c_extract_double(text, "msg3_snr_db", &double_value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing floating config key", "msg3_snr_db"); }
  out_config->sim.msg3_snr_db = double_value;
  if (mini_gnb_c_extract_double(text, "msg3_evm", &double_value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing floating config key", "msg3_evm"); }
  out_config->sim.msg3_evm = double_value;
  if (mini_gnb_c_extract_string(text, "contention_id_hex", out_config->sim.contention_id_hex, sizeof(out_config->sim.contention_id_hex)) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing string config key", "contention_id_hex"); }
  if (mini_gnb_c_extract_int(text, "establishment_cause", &value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing integer config key", "establishment_cause"); }
  out_config->sim.establishment_cause = (uint8_t)value;
  if (mini_gnb_c_extract_int(text, "ue_identity_type", &value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing integer config key", "ue_identity_type"); }
  out_config->sim.ue_identity_type = (uint8_t)value;
  if (mini_gnb_c_extract_string(text, "ue_identity_hex", out_config->sim.ue_identity_hex, sizeof(out_config->sim.ue_identity_hex)) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing string config key", "ue_identity_hex"); }
  if (mini_gnb_c_extract_bool(text, "include_crnti_ce", &bool_value) != 0) { free(text); return mini_gnb_c_fail(error_message, error_message_size, "missing boolean config key", "include_crnti_ce"); }
  out_config->sim.include_crnti_ce = bool_value;

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
               "  ss0_index=%u coreset0_index=%u\n"
               "RA config summary:\n"
               "  prach_config_index=%u root_seq=%u zero_corr=%u ra_resp_window=%u msg3_delta_preamble=%d\n"
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
               config->prach.prach_config_index,
               config->prach.prach_root_seq_index,
               config->prach.zero_correlation_zone,
               config->prach.ra_resp_window,
               config->prach.msg3_delta_preamble,
               config->rf.device_driver,
               config->rf.clock_src,
               config->rf.srate,
               config->rf.tx_gain,
               config->rf.rx_gain) >= (int)out_size) {
    return -1;
  }

  return 0;
}
