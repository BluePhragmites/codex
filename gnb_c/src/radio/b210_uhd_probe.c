#define _GNU_SOURCE

#include "mini_gnb_c/radio/b210_uhd_probe.h"
#include "mini_gnb_c/radio/host_performance.h"
#include "mini_gnb_c/radio/sc16_ring_map.h"

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef __linux__
#include <sched.h>
#endif

#include <uhd.h>

#define MINI_GNB_C_B210_MAX_CHANNELS 2u

static int mini_gnb_c_b210_probe_fail(char* error_message,
                                      const size_t error_message_size,
                                      const char* message) {
  if (error_message != NULL && error_message_size > 0u) {
    (void)snprintf(error_message, error_message_size, "%s", message != NULL ? message : "unknown error");
  }
  return -1;
}

static int mini_gnb_c_b210_probe_failf(char* error_message,
                                       const size_t error_message_size,
                                       const char* prefix,
                                       const char* detail) {
  if (error_message != NULL && error_message_size > 0u) {
    (void)snprintf(error_message,
                   error_message_size,
                   "%s%s%s",
                   prefix != NULL ? prefix : "error",
                   (detail != NULL && detail[0] != '\0') ? ": " : "",
                   detail != NULL ? detail : "");
  }
  return -1;
}

static int mini_gnb_c_b210_probe_fill_uhd_error(uhd_usrp_handle usrp,
                                                const char* prefix,
                                                char* error_message,
                                                const size_t error_message_size) {
  char detail[512];

  detail[0] = '\0';
  if (usrp != NULL) {
    (void)uhd_usrp_last_error(usrp, detail, sizeof(detail));
  }
  return mini_gnb_c_b210_probe_failf(error_message, error_message_size, prefix, detail);
}

static int mini_gnb_c_b210_probe_fill_tx_stream_error(uhd_tx_streamer_handle tx_streamer,
                                                      const char* prefix,
                                                      char* error_message,
                                                      const size_t error_message_size) {
  char detail[512];

  detail[0] = '\0';
  if (tx_streamer != NULL) {
    (void)uhd_tx_streamer_last_error(tx_streamer, detail, sizeof(detail));
  }
  return mini_gnb_c_b210_probe_failf(error_message, error_message_size, prefix, detail);
}

static int mini_gnb_c_b210_probe_pin_current_thread(const int cpu_core,
                                                    char* error_message,
                                                    const size_t error_message_size) {
#ifdef __linux__
  cpu_set_t set;

  if (cpu_core < 0) {
    return 0;
  }
  if (cpu_core >= CPU_SETSIZE) {
    char detail[128];

    (void)snprintf(detail, sizeof(detail), "invalid CPU core %d", cpu_core);
    return mini_gnb_c_b210_probe_fail(error_message, error_message_size, detail);
  }
  CPU_ZERO(&set);
  CPU_SET(cpu_core, &set);
  if (sched_setaffinity(0, sizeof(set), &set) != 0) {
    char detail[128];

    (void)snprintf(detail, sizeof(detail), "failed to pin current thread to CPU %d: %s", cpu_core, strerror(errno));
    return mini_gnb_c_b210_probe_fail(error_message, error_message_size, detail);
  }
  return 0;
#else
  (void)cpu_core;
  return mini_gnb_c_b210_probe_fail(error_message,
                                    error_message_size,
                                    "CPU affinity for the B210 probe is only supported on Linux");
#endif
}

static uint64_t mini_gnb_c_b210_probe_now_monotonic_ns(void) {
  struct timespec ts;

  memset(&ts, 0, sizeof(ts));
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return 0u;
  }
  return ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;
}

static int mini_gnb_c_b210_probe_rx_metadata_time_ns(uhd_rx_metadata_handle md,
                                                     uint64_t* time_ns_out,
                                                     bool* valid_out,
                                                     char* error_message,
                                                     const size_t error_message_size) {
  bool has_time_spec = false;
  int64_t full_secs = 0;
  double frac_secs = 0.0;

  if (time_ns_out != NULL) {
    *time_ns_out = 0u;
  }
  if (valid_out != NULL) {
    *valid_out = false;
  }
  if (md == NULL || time_ns_out == NULL || valid_out == NULL) {
    return mini_gnb_c_b210_probe_fail(error_message, error_message_size, "invalid RX metadata time request");
  }
  if (uhd_rx_metadata_has_time_spec(md, &has_time_spec) != UHD_ERROR_NONE) {
    return mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to query RX metadata time presence");
  }
  if (!has_time_spec) {
    return 0;
  }
  if (uhd_rx_metadata_time_spec(md, &full_secs, &frac_secs) != UHD_ERROR_NONE) {
    return mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to read RX metadata time spec");
  }
  *time_ns_out = mini_gnb_c_b210_time_spec_to_ns(full_secs, frac_secs);
  *valid_out = true;
  return 0;
}

static bool mini_gnb_c_b210_probe_string_vector_contains(uhd_string_vector_handle vector, const char* value) {
  size_t count = 0u;
  size_t i = 0u;
  char item[128];

  if (vector == NULL || value == NULL || value[0] == '\0') {
    return false;
  }
  if (uhd_string_vector_size(vector, &count) != UHD_ERROR_NONE) {
    return false;
  }
  for (i = 0u; i < count; ++i) {
    if (uhd_string_vector_at(vector, i, item, sizeof(item)) == UHD_ERROR_NONE && strcmp(item, value) == 0) {
      return true;
    }
  }
  return false;
}

static int mini_gnb_c_b210_probe_optional_sensor_bool(uhd_usrp_handle usrp,
                                                      const bool mboard_sensor,
                                                      const char* sensor_name,
                                                      const size_t index,
                                                      bool* valid_out,
                                                      bool* value_out,
                                                      char* error_message,
                                                      const size_t error_message_size) {
  uhd_string_vector_handle names = NULL;
  uhd_sensor_value_handle sensor = NULL;
  bool value = false;
  int rc = -1;

  if (valid_out != NULL) {
    *valid_out = false;
  }
  if (value_out != NULL) {
    *value_out = false;
  }
  if (usrp == NULL || sensor_name == NULL || valid_out == NULL || value_out == NULL) {
    return -1;
  }

  if (uhd_string_vector_make(&names) != UHD_ERROR_NONE) {
    return mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to allocate sensor name list");
  }
  if ((mboard_sensor ? uhd_usrp_get_mboard_sensor_names(usrp, index, &names)
                     : uhd_usrp_get_rx_sensor_names(usrp, index, &names)) != UHD_ERROR_NONE) {
    goto cleanup;
  }
  if (!mini_gnb_c_b210_probe_string_vector_contains(names, sensor_name)) {
    rc = 0;
    goto cleanup;
  }
  if (uhd_sensor_value_make(&sensor) != UHD_ERROR_NONE) {
    goto cleanup;
  }
  if ((mboard_sensor ? uhd_usrp_get_mboard_sensor(usrp, sensor_name, index, &sensor)
                     : uhd_usrp_get_rx_sensor(usrp, sensor_name, index, &sensor)) != UHD_ERROR_NONE) {
    goto cleanup;
  }
  if (uhd_sensor_value_to_bool(sensor, &value) != UHD_ERROR_NONE) {
    goto cleanup;
  }
  *valid_out = true;
  *value_out = value;
  rc = 0;

cleanup:
  if (sensor != NULL) {
    uhd_sensor_value_free(&sensor);
  }
  if (names != NULL) {
    uhd_string_vector_free(&names);
  }
  if (rc != 0) {
    char detail[128];

    (void)snprintf(detail, sizeof(detail), "failed to query sensor %s", sensor_name);
    return mini_gnb_c_b210_probe_fail(error_message, error_message_size, detail);
  }
  return 0;
}

static int mini_gnb_c_b210_probe_optional_tx_sensor_bool(uhd_usrp_handle usrp,
                                                         const char* sensor_name,
                                                         const size_t channel,
                                                         bool* valid_out,
                                                         bool* value_out,
                                                         char* error_message,
                                                         const size_t error_message_size) {
  uhd_string_vector_handle names = NULL;
  uhd_sensor_value_handle sensor = NULL;
  bool value = false;
  int rc = -1;

  if (valid_out != NULL) {
    *valid_out = false;
  }
  if (value_out != NULL) {
    *value_out = false;
  }
  if (usrp == NULL || sensor_name == NULL || valid_out == NULL || value_out == NULL) {
    return -1;
  }

  if (uhd_string_vector_make(&names) != UHD_ERROR_NONE) {
    return mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to allocate TX sensor name list");
  }
  if (uhd_usrp_get_tx_sensor_names(usrp, channel, &names) != UHD_ERROR_NONE) {
    goto cleanup;
  }
  if (!mini_gnb_c_b210_probe_string_vector_contains(names, sensor_name)) {
    rc = 0;
    goto cleanup;
  }
  if (uhd_sensor_value_make(&sensor) != UHD_ERROR_NONE) {
    goto cleanup;
  }
  if (uhd_usrp_get_tx_sensor(usrp, sensor_name, channel, &sensor) != UHD_ERROR_NONE) {
    goto cleanup;
  }
  if (uhd_sensor_value_to_bool(sensor, &value) != UHD_ERROR_NONE) {
    goto cleanup;
  }
  *valid_out = true;
  *value_out = value;
  rc = 0;

cleanup:
  if (sensor != NULL) {
    uhd_sensor_value_free(&sensor);
  }
  if (names != NULL) {
    uhd_string_vector_free(&names);
  }
  if (rc != 0) {
    char detail[128];

    (void)snprintf(detail, sizeof(detail), "failed to query TX sensor %s", sensor_name);
    return mini_gnb_c_b210_probe_fail(error_message, error_message_size, detail);
  }
  return 0;
}

static int mini_gnb_c_b210_probe_fill_channel_list(const size_t first_channel,
                                                   const uint32_t channel_count,
                                                   size_t* channel_list,
                                                   char* error_message,
                                                   const size_t error_message_size) {
  uint32_t i = 0u;

  if (channel_count == 0u || channel_count > MINI_GNB_C_B210_MAX_CHANNELS || channel_list == NULL) {
    return mini_gnb_c_b210_probe_fail(error_message, error_message_size, "invalid channel count");
  }
  if (first_channel + channel_count > MINI_GNB_C_B210_MAX_CHANNELS) {
    return mini_gnb_c_b210_probe_fail(error_message, error_message_size, "channel range exceeds B210 channel count");
  }
  for (i = 0u; i < channel_count; ++i) {
    channel_list[i] = first_channel + i;
  }
  return 0;
}

static int mini_gnb_c_b210_probe_apply_rx_profile(uhd_usrp_handle usrp,
                                                  const size_t first_channel,
                                                  const uint32_t channel_count,
                                                  const double rate_sps,
                                                  const double gain_db,
                                                  const double bandwidth_hz,
                                                  const double freq_hz,
                                                  char* error_message,
                                                  const size_t error_message_size) {
  uhd_tune_request_t tune_request;
  uhd_tune_result_t tune_result;
  uint32_t i = 0u;

  memset(&tune_request, 0, sizeof(tune_request));
  memset(&tune_result, 0, sizeof(tune_result));
  tune_request.target_freq = freq_hz;
  tune_request.rf_freq_policy = UHD_TUNE_REQUEST_POLICY_AUTO;
  tune_request.dsp_freq_policy = UHD_TUNE_REQUEST_POLICY_AUTO;
  for (i = 0u; i < channel_count; ++i) {
    const size_t channel = first_channel + i;

    if (uhd_usrp_set_rx_rate(usrp, rate_sps, channel) != UHD_ERROR_NONE ||
        uhd_usrp_set_rx_gain(usrp, gain_db, channel, "") != UHD_ERROR_NONE ||
        uhd_usrp_set_rx_bandwidth(usrp, bandwidth_hz, channel) != UHD_ERROR_NONE ||
        uhd_usrp_set_rx_freq(usrp, &tune_request, channel, &tune_result) != UHD_ERROR_NONE) {
      return mini_gnb_c_b210_probe_fill_uhd_error(usrp, "failed to apply RX settings", error_message, error_message_size);
    }
  }
  return 0;
}

static int mini_gnb_c_b210_probe_apply_tx_profile(uhd_usrp_handle usrp,
                                                  const size_t first_channel,
                                                  const uint32_t channel_count,
                                                  const double rate_sps,
                                                  const double gain_db,
                                                  const double bandwidth_hz,
                                                  const double freq_hz,
                                                  char* error_message,
                                                  const size_t error_message_size) {
  uhd_tune_request_t tune_request;
  uhd_tune_result_t tune_result;
  uint32_t i = 0u;

  memset(&tune_request, 0, sizeof(tune_request));
  memset(&tune_result, 0, sizeof(tune_result));
  tune_request.target_freq = freq_hz;
  tune_request.rf_freq_policy = UHD_TUNE_REQUEST_POLICY_AUTO;
  tune_request.dsp_freq_policy = UHD_TUNE_REQUEST_POLICY_AUTO;
  for (i = 0u; i < channel_count; ++i) {
    const size_t channel = first_channel + i;

    if (uhd_usrp_set_tx_rate(usrp, rate_sps, channel) != UHD_ERROR_NONE ||
        uhd_usrp_set_tx_gain(usrp, gain_db, channel, "") != UHD_ERROR_NONE ||
        uhd_usrp_set_tx_bandwidth(usrp, bandwidth_hz, channel) != UHD_ERROR_NONE ||
        uhd_usrp_set_tx_freq(usrp, &tune_request, channel, &tune_result) != UHD_ERROR_NONE) {
      return mini_gnb_c_b210_probe_fill_uhd_error(usrp, "failed to apply TX settings", error_message, error_message_size);
    }
  }
  return 0;
}

static void mini_gnb_c_b210_probe_collect_rx_lo_lock(uhd_usrp_handle usrp,
                                                     const size_t first_channel,
                                                     const uint32_t channel_count,
                                                     bool* valid_out,
                                                     bool* locked_out) {
  uint32_t i = 0u;
  bool any_valid = false;
  bool all_locked = true;

  if (valid_out != NULL) {
    *valid_out = false;
  }
  if (locked_out != NULL) {
    *locked_out = false;
  }
  for (i = 0u; i < channel_count; ++i) {
    bool valid = false;
    bool locked = false;

    if (mini_gnb_c_b210_probe_optional_sensor_bool(usrp,
                                                   false,
                                                   "lo_locked",
                                                   first_channel + i,
                                                   &valid,
                                                   &locked,
                                                   NULL,
                                                   0u) != 0) {
      continue;
    }
    if (valid) {
      any_valid = true;
      if (!locked) {
        all_locked = false;
      }
    }
  }
  if (valid_out != NULL) {
    *valid_out = any_valid;
  }
  if (locked_out != NULL) {
    *locked_out = any_valid ? all_locked : false;
  }
}

static void mini_gnb_c_b210_probe_collect_tx_lo_lock(uhd_usrp_handle usrp,
                                                     const size_t first_channel,
                                                     const uint32_t channel_count,
                                                     bool* valid_out,
                                                     bool* locked_out) {
  uint32_t i = 0u;
  bool any_valid = false;
  bool all_locked = true;

  if (valid_out != NULL) {
    *valid_out = false;
  }
  if (locked_out != NULL) {
    *locked_out = false;
  }
  for (i = 0u; i < channel_count; ++i) {
    bool valid = false;
    bool locked = false;

    if (mini_gnb_c_b210_probe_optional_tx_sensor_bool(usrp,
                                                      "lo_locked",
                                                      first_channel + i,
                                                      &valid,
                                                      &locked,
                                                      NULL,
                                                      0u) != 0) {
      continue;
    }
    if (valid) {
      any_valid = true;
      if (!locked) {
        all_locked = false;
      }
    }
  }
  if (valid_out != NULL) {
    *valid_out = any_valid;
  }
  if (locked_out != NULL) {
    *locked_out = any_valid ? all_locked : false;
  }
}

static int mini_gnb_c_b210_probe_issue_rx_start(uhd_usrp_handle usrp,
                                                uhd_rx_streamer_handle rx_streamer,
                                                const uint32_t channel_count,
                                                char* error_message,
                                                const size_t error_message_size) {
  uhd_stream_cmd_t stream_cmd;

  memset(&stream_cmd, 0, sizeof(stream_cmd));
  stream_cmd.stream_mode = UHD_STREAM_MODE_START_CONTINUOUS;
  if (channel_count > 1u) {
    int64_t full_secs = 0;
    double frac_secs = 0.0;

    if (uhd_usrp_get_time_now(usrp, 0u, &full_secs, &frac_secs) != UHD_ERROR_NONE) {
      return mini_gnb_c_b210_probe_fill_uhd_error(usrp, "failed to get current USRP time", error_message, error_message_size);
    }
    frac_secs += 0.05;
    if (frac_secs >= 1.0) {
      frac_secs -= 1.0;
      full_secs += 1;
    }
    stream_cmd.stream_now = false;
    stream_cmd.time_spec_full_secs = full_secs;
    stream_cmd.time_spec_frac_secs = frac_secs;
  } else {
    stream_cmd.stream_now = true;
  }
  if (uhd_rx_streamer_issue_stream_cmd(rx_streamer, &stream_cmd) != UHD_ERROR_NONE) {
    return mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to issue RX stream command");
  }
  return 0;
}

typedef struct {
  uhd_rx_streamer_handle rx_streamer;
  size_t requested_samples;
  size_t samples_per_buffer;
  double rate_sps;
  mini_gnb_c_b210_duration_mode_t duration_mode;
  uint64_t deadline_ns;
  uint32_t channel_count;
  int cpu_core;
  uint32_t ring_block_samples;
  mini_gnb_c_sc16_ring_map_t* ring;
  size_t received_samples;
  size_t ring_blocks_committed;
  size_t rx_overflow_events;
  size_t rx_timeout_events;
  size_t rx_recoverable_events;
  size_t rx_gap_events;
  size_t rx_lost_samples_estimate;
  uint64_t prev_expected_next_hw_time_ns;
  int rc;
  char error_message[256];
} mini_gnb_c_b210_trx_rx_worker_t;

typedef struct {
  uhd_tx_streamer_handle tx_streamer;
  size_t requested_samples;
  size_t samples_per_buffer;
  uint32_t channel_count;
  int cpu_core;
  mini_gnb_c_sc16_ring_map_t* tx_ring;
  size_t tx_prefetch_samples;
  uint64_t tx_ring_start_seq;
  uint64_t tx_ring_end_seq;
  bool loop_tx_ring;
  size_t transmitted_samples;
  size_t ring_blocks_consumed;
  size_t tx_ring_wrap_count;
  bool burst_ack_valid;
  bool burst_ack;
  bool underflow_observed;
  bool seq_error_observed;
  bool time_error_observed;
  int rc;
  char error_message[256];
} mini_gnb_c_b210_trx_tx_worker_t;

static int mini_gnb_c_b210_fill_tx_prefetch_from_ring(mini_gnb_c_sc16_ring_map_t* ring,
                                                      const uint32_t channel_count,
                                                      int16_t* sc16_prefetch,
                                                      const size_t prefetch_capacity_samples,
                                                      const uint64_t ring_start_seq,
                                                      const uint64_t ring_end_seq,
                                                      const bool loop_ring,
                                                      uint64_t* prefetch_seq,
                                                      size_t* prefetch_block_offset_samples,
                                                      size_t* prefetch_valid_samples,
                                                      size_t* prefetch_offset_samples,
                                                      size_t* ring_blocks_consumed,
                                                      size_t* ring_wrap_count,
                                                      char* error_message,
                                                      const size_t error_message_size) {
  uint32_t channel_index = 0u;

  if (ring == NULL || sc16_prefetch == NULL || prefetch_seq == NULL || prefetch_block_offset_samples == NULL ||
      prefetch_valid_samples == NULL || prefetch_offset_samples == NULL || ring_blocks_consumed == NULL) {
    return mini_gnb_c_b210_probe_fail(error_message, error_message_size, "invalid TX prefetch request");
  }

  if (*prefetch_valid_samples > 0u && *prefetch_offset_samples > 0u) {
    for (channel_index = 0u; channel_index < channel_count; ++channel_index) {
      int16_t* channel_base = sc16_prefetch + ((size_t)channel_index * prefetch_capacity_samples * 2u);

      memmove(channel_base,
              channel_base + (*prefetch_offset_samples * 2u),
              *prefetch_valid_samples * 2u * sizeof(int16_t));
    }
  }
  *prefetch_offset_samples = 0u;

  while (*prefetch_valid_samples < prefetch_capacity_samples) {
    const mini_gnb_c_sc16_ring_map_descriptor_t* descriptor =
        NULL;
    size_t available_in_block = 0u;
    size_t samples_to_copy = 0u;

    if (*prefetch_seq >= ring_end_seq) {
      if (!loop_ring) {
        break;
      }
      *prefetch_seq = ring_start_seq;
      *prefetch_block_offset_samples = 0u;
      if (ring_wrap_count != NULL) {
        *ring_wrap_count += 1u;
      }
    }
    descriptor = mini_gnb_c_sc16_ring_map_get_descriptor(ring, *prefetch_seq);

    if (descriptor == NULL || descriptor->sample_count == 0u) {
      *prefetch_seq += 1u;
      *prefetch_block_offset_samples = 0u;
      continue;
    }
    if (*prefetch_block_offset_samples >= descriptor->sample_count) {
      *prefetch_seq += 1u;
      *prefetch_block_offset_samples = 0u;
      continue;
    }
    available_in_block = descriptor->sample_count - *prefetch_block_offset_samples;
    samples_to_copy = prefetch_capacity_samples - *prefetch_valid_samples;
    if (samples_to_copy > available_in_block) {
      samples_to_copy = available_in_block;
    }
    for (channel_index = 0u; channel_index < channel_count; ++channel_index) {
      const int16_t* channel_payload =
          mini_gnb_c_sc16_ring_map_get_channel_payload(ring, *prefetch_seq, channel_index);
      int16_t* staging_channel =
          sc16_prefetch + (((size_t)channel_index * prefetch_capacity_samples + *prefetch_valid_samples) * 2u);

      if (channel_payload == NULL) {
        return mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to resolve TX ring payload");
      }
      memcpy(staging_channel,
             channel_payload + (*prefetch_block_offset_samples * 2u),
             samples_to_copy * 2u * sizeof(int16_t));
    }
    *prefetch_valid_samples += samples_to_copy;
    *prefetch_block_offset_samples += samples_to_copy;
    if (*prefetch_block_offset_samples == descriptor->sample_count) {
      *prefetch_seq += 1u;
      *prefetch_block_offset_samples = 0u;
      *ring_blocks_consumed += 1u;
    }
  }
  return 0;
}

static bool mini_gnb_c_b210_is_recoverable_rx_metadata_error(const uhd_rx_metadata_error_code_t error_code) {
  return error_code == UHD_RX_METADATA_ERROR_CODE_OVERFLOW || error_code == UHD_RX_METADATA_ERROR_CODE_TIMEOUT;
}

static bool mini_gnb_c_b210_duration_deadline_reached(const mini_gnb_c_b210_duration_mode_t duration_mode,
                                                      const uint64_t deadline_ns) {
  if (duration_mode != MINI_GNB_C_B210_DURATION_MODE_WALLCLOCK || deadline_ns == 0u) {
    return false;
  }
  return mini_gnb_c_b210_probe_now_monotonic_ns() >= deadline_ns;
}

static size_t mini_gnb_c_b210_estimate_lost_samples_from_gap(const uint64_t expected_next_hw_time_ns,
                                                             const uint64_t current_hw_time_ns,
                                                             const double rate_sps) {
  double lost_samples = 0.0;

  if (expected_next_hw_time_ns == 0u || current_hw_time_ns <= expected_next_hw_time_ns || rate_sps <= 0.0) {
    return 0u;
  }
  lost_samples = ((double)(current_hw_time_ns - expected_next_hw_time_ns) * rate_sps) / 1000000000.0;
  if (lost_samples <= 0.5) {
    return 0u;
  }
  return (size_t)(lost_samples + 0.5);
}

static double mini_gnb_c_b210_compute_rx_timeout_sec(const mini_gnb_c_b210_duration_mode_t duration_mode,
                                                     const uint64_t deadline_ns,
                                                     const double default_timeout_sec) {
  const uint64_t now_ns = mini_gnb_c_b210_probe_now_monotonic_ns();
  double timeout_sec = default_timeout_sec;

  if (duration_mode != MINI_GNB_C_B210_DURATION_MODE_WALLCLOCK || deadline_ns == 0u || now_ns == 0u ||
      now_ns >= deadline_ns) {
    return default_timeout_sec;
  }
  timeout_sec = (double)(deadline_ns - now_ns) / 1000000000.0;
  if (timeout_sec < 0.001) {
    timeout_sec = 0.001;
  }
  if (timeout_sec > default_timeout_sec) {
    timeout_sec = default_timeout_sec;
  }
  return timeout_sec;
}

static void* mini_gnb_c_b210_trx_rx_worker_main(void* arg) {
  mini_gnb_c_b210_trx_rx_worker_t* ctx = (mini_gnb_c_b210_trx_rx_worker_t*)arg;
  uhd_rx_metadata_handle md = NULL;
  int16_t* channel_buffers[MINI_GNB_C_B210_MAX_CHANNELS] = {NULL, NULL};
  void* buffer_ptrs[MINI_GNB_C_B210_MAX_CHANNELS] = {NULL, NULL};
  int16_t* ring_staging = NULL;
  size_t ring_staging_samples = 0u;
  uint64_t ring_staging_hw_time_ns = 0u;
  uint32_t ring_staging_flags = 0u;
  uint32_t channel_index = 0u;

  if (ctx == NULL || ctx->ring == NULL) {
    return NULL;
  }
  ctx->rc = -1;
  if (mini_gnb_c_b210_probe_pin_current_thread(ctx->cpu_core, ctx->error_message, sizeof(ctx->error_message)) != 0) {
    return NULL;
  }
  if (uhd_rx_metadata_make(&md) != UHD_ERROR_NONE) {
    (void)snprintf(ctx->error_message, sizeof(ctx->error_message), "%s", "failed to allocate TRX RX metadata");
    return NULL;
  }
  for (channel_index = 0u; channel_index < ctx->channel_count; ++channel_index) {
    channel_buffers[channel_index] = (int16_t*)malloc(ctx->samples_per_buffer * 2u * sizeof(int16_t));
    if (channel_buffers[channel_index] == NULL) {
      (void)snprintf(ctx->error_message, sizeof(ctx->error_message), "%s", "failed to allocate TRX RX channel buffer");
      goto cleanup;
    }
    buffer_ptrs[channel_index] = channel_buffers[channel_index];
  }
  ring_staging =
      (int16_t*)malloc((size_t)ctx->ring_block_samples * ctx->channel_count * 2u * sizeof(int16_t));
  if (ring_staging == NULL) {
    (void)snprintf(ctx->error_message, sizeof(ctx->error_message), "%s", "failed to allocate TRX RX ring staging");
    goto cleanup;
  }

  while (!mini_gnb_c_b210_duration_deadline_reached(ctx->duration_mode, ctx->deadline_ns) &&
         (ctx->duration_mode == MINI_GNB_C_B210_DURATION_MODE_WALLCLOCK || ctx->received_samples < ctx->requested_samples)) {
    size_t got = 0u;
    size_t samples_to_write = 0u;
    uhd_rx_metadata_error_code_t error_code = UHD_RX_METADATA_ERROR_CODE_NONE;
    const double recv_timeout_sec =
        mini_gnb_c_b210_compute_rx_timeout_sec(ctx->duration_mode, ctx->deadline_ns, 3.0);

    if (uhd_rx_streamer_recv(ctx->rx_streamer,
                             buffer_ptrs,
                             ctx->samples_per_buffer,
                             &md,
                             recv_timeout_sec,
                             false,
                             &got) != UHD_ERROR_NONE) {
      (void)snprintf(ctx->error_message, sizeof(ctx->error_message), "%s", "failed while receiving TRX RX samples");
      goto cleanup;
    }
    if (uhd_rx_metadata_error_code(md, &error_code) != UHD_ERROR_NONE) {
      (void)snprintf(ctx->error_message, sizeof(ctx->error_message), "%s", "failed to read TRX RX metadata");
      goto cleanup;
    }
    if (error_code != UHD_RX_METADATA_ERROR_CODE_NONE) {
      if (mini_gnb_c_b210_is_recoverable_rx_metadata_error(error_code)) {
        if (error_code == UHD_RX_METADATA_ERROR_CODE_OVERFLOW) {
          ctx->rx_overflow_events += 1u;
        } else if (error_code == UHD_RX_METADATA_ERROR_CODE_TIMEOUT) {
          ctx->rx_timeout_events += 1u;
        }
        ctx->rx_recoverable_events += 1u;
        continue;
      }
      (void)snprintf(ctx->error_message,
                     sizeof(ctx->error_message),
                     "UHD TRX RX metadata error 0x%x",
                     (unsigned)error_code);
      goto cleanup;
    }
    if (got == 0u) {
      continue;
    }
    if (ctx->duration_mode == MINI_GNB_C_B210_DURATION_MODE_WALLCLOCK) {
      samples_to_write = got;
    } else {
      const size_t remaining = ctx->requested_samples - ctx->received_samples;

      samples_to_write = got > remaining ? remaining : got;
    }
    {
      uint64_t recv_hw_time_ns = 0u;
      bool recv_hw_time_valid = false;
      size_t offset_samples = 0u;

      if (mini_gnb_c_b210_probe_rx_metadata_time_ns(md,
                                                    &recv_hw_time_ns,
                                                    &recv_hw_time_valid,
                                                    ctx->error_message,
                                                    sizeof(ctx->error_message)) != 0) {
        goto cleanup;
      }
      if (recv_hw_time_valid) {
        const size_t lost_samples = mini_gnb_c_b210_estimate_lost_samples_from_gap(ctx->prev_expected_next_hw_time_ns,
                                                                                    recv_hw_time_ns,
                                                                                    ctx->rate_sps);

        if (lost_samples > 0u) {
          ctx->rx_gap_events += 1u;
          ctx->rx_lost_samples_estimate += lost_samples;
        }
      }

      while (offset_samples < samples_to_write) {
        const size_t free_samples = ctx->ring_block_samples - ring_staging_samples;
        const size_t copy_samples =
            (samples_to_write - offset_samples) > free_samples ? free_samples : (samples_to_write - offset_samples);

        if (ring_staging_samples == 0u) {
          if (recv_hw_time_valid) {
            ring_staging_hw_time_ns =
                mini_gnb_c_b210_advance_time_ns(recv_hw_time_ns, offset_samples, ctx->rate_sps);
            ring_staging_flags = MINI_GNB_C_SC16_RING_FLAG_HW_TIME_VALID;
          } else {
            ring_staging_hw_time_ns = mini_gnb_c_b210_probe_now_monotonic_ns();
            ring_staging_flags = MINI_GNB_C_SC16_RING_FLAG_HOST_TIME_FALLBACK;
          }
        }

        for (channel_index = 0u; channel_index < ctx->channel_count; ++channel_index) {
          memcpy(ring_staging + (((size_t)channel_index * ctx->ring_block_samples + ring_staging_samples) * 2u),
                 channel_buffers[channel_index] + (offset_samples * 2u),
                 copy_samples * 2u * sizeof(int16_t));
        }
        ring_staging_samples += copy_samples;
        offset_samples += copy_samples;

        if (ring_staging_samples == ctx->ring_block_samples) {
          if (mini_gnb_c_sc16_ring_map_append(ctx->ring,
                                              ring_staging,
                                              (uint32_t)ring_staging_samples,
                                              ring_staging_hw_time_ns,
                                              ring_staging_flags,
                                              ctx->error_message,
                                              sizeof(ctx->error_message)) != 0) {
            goto cleanup;
          }
          ctx->ring_blocks_committed += 1u;
          ring_staging_samples = 0u;
          ring_staging_hw_time_ns = 0u;
          ring_staging_flags = 0u;
        }
      }
      if (recv_hw_time_valid) {
        ctx->prev_expected_next_hw_time_ns =
            mini_gnb_c_b210_advance_time_ns(recv_hw_time_ns, samples_to_write, ctx->rate_sps);
      }
    }
    ctx->received_samples += samples_to_write;
  }
  if (ring_staging_samples > 0u) {
    if (mini_gnb_c_sc16_ring_map_append(ctx->ring,
                                        ring_staging,
                                        (uint32_t)ring_staging_samples,
                                        ring_staging_hw_time_ns,
                                        ring_staging_flags,
                                        ctx->error_message,
                                        sizeof(ctx->error_message)) != 0) {
      goto cleanup;
    }
    ctx->ring_blocks_committed += 1u;
  }
  ctx->rc = 0;

cleanup:
  if (ring_staging != NULL) {
    free(ring_staging);
  }
  for (channel_index = 0u; channel_index < MINI_GNB_C_B210_MAX_CHANNELS; ++channel_index) {
    if (channel_buffers[channel_index] != NULL) {
      free(channel_buffers[channel_index]);
    }
  }
  if (md != NULL) {
    uhd_rx_metadata_free(&md);
  }
  return NULL;
}

static void* mini_gnb_c_b210_trx_tx_worker_main(void* arg) {
  mini_gnb_c_b210_trx_tx_worker_t* ctx = (mini_gnb_c_b210_trx_tx_worker_t*)arg;
  uhd_async_metadata_handle async_md = NULL;
  uhd_tx_metadata_handle only_md = NULL;
  uhd_tx_metadata_handle start_md = NULL;
  uhd_tx_metadata_handle middle_md = NULL;
  uhd_tx_metadata_handle end_md = NULL;
  int16_t* sc16_prefetch = NULL;
  size_t prefetch_capacity_samples = 0u;
  size_t prefetch_valid_samples = 0u;
  size_t prefetch_offset_samples = 0u;
  size_t prefetch_block_offset_samples = 0u;
  uint64_t prefetch_seq = 0u;
  const size_t prefetch_low_watermark_divisor = 2u;

  if (ctx == NULL || ctx->tx_ring == NULL) {
    return NULL;
  }
  ctx->rc = -1;
  if (mini_gnb_c_b210_probe_pin_current_thread(ctx->cpu_core, ctx->error_message, sizeof(ctx->error_message)) != 0) {
    return NULL;
  }
  if (uhd_async_metadata_make(&async_md) != UHD_ERROR_NONE ||
      uhd_tx_metadata_make(&only_md, false, 0, 0.0, true, true) != UHD_ERROR_NONE ||
      uhd_tx_metadata_make(&start_md, false, 0, 0.0, true, false) != UHD_ERROR_NONE ||
      uhd_tx_metadata_make(&middle_md, false, 0, 0.0, false, false) != UHD_ERROR_NONE ||
      uhd_tx_metadata_make(&end_md, false, 0, 0.0, false, true) != UHD_ERROR_NONE) {
    (void)snprintf(ctx->error_message, sizeof(ctx->error_message), "%s", "failed to allocate TRX TX metadata");
    goto cleanup;
  }
  prefetch_seq = ctx->tx_ring_start_seq;
  prefetch_capacity_samples = ctx->tx_prefetch_samples;
  if (prefetch_capacity_samples == 0u) {
    prefetch_capacity_samples = ctx->samples_per_buffer * 64u;
  }
  if (prefetch_capacity_samples < ctx->tx_ring->superblock->block_samples) {
    prefetch_capacity_samples = ctx->tx_ring->superblock->block_samples;
  }
  if (prefetch_capacity_samples < ctx->samples_per_buffer) {
    prefetch_capacity_samples = ctx->samples_per_buffer;
  }
  if (prefetch_capacity_samples > ctx->requested_samples) {
    prefetch_capacity_samples = ctx->requested_samples;
  }
  if (prefetch_capacity_samples == 0u) {
    prefetch_capacity_samples = ctx->samples_per_buffer;
  }
  sc16_prefetch =
      (int16_t*)malloc(prefetch_capacity_samples * ctx->channel_count * 2u * sizeof(int16_t));
  if (sc16_prefetch == NULL) {
    (void)snprintf(ctx->error_message, sizeof(ctx->error_message), "%s", "failed to allocate TRX TX prefetch buffer");
    goto cleanup;
  }

  while (ctx->transmitted_samples < ctx->requested_samples) {
    const size_t prefetch_low_watermark = prefetch_capacity_samples / prefetch_low_watermark_divisor;
    size_t samples_to_send = 0u;
    size_t samples_sent = 0u;
    const void* buffer_ptrs[MINI_GNB_C_B210_MAX_CHANNELS] = {NULL, NULL};
    uhd_tx_metadata_handle md = middle_md;
    uint32_t channel_index = 0u;

      if (prefetch_valid_samples <= prefetch_low_watermark) {
      if (mini_gnb_c_b210_fill_tx_prefetch_from_ring(ctx->tx_ring,
                                                     ctx->channel_count,
                                                     sc16_prefetch,
                                                     prefetch_capacity_samples,
                                                     ctx->tx_ring_start_seq,
                                                     ctx->tx_ring_end_seq,
                                                     ctx->loop_tx_ring,
                                                     &prefetch_seq,
                                                     &prefetch_block_offset_samples,
                                                     &prefetch_valid_samples,
                                                     &prefetch_offset_samples,
                                                     &ctx->ring_blocks_consumed,
                                                     &ctx->tx_ring_wrap_count,
                                                     ctx->error_message,
                                                     sizeof(ctx->error_message)) != 0) {
        goto cleanup;
      }
    }
    if (prefetch_valid_samples == 0u) {
      (void)snprintf(ctx->error_message, sizeof(ctx->error_message), "%s", "TRX TX prefetch buffer ran empty");
      goto cleanup;
    }

    samples_to_send = ctx->requested_samples - ctx->transmitted_samples;
    if (samples_to_send > ctx->samples_per_buffer) {
      samples_to_send = ctx->samples_per_buffer;
    }
    if (samples_to_send > prefetch_valid_samples) {
      samples_to_send = prefetch_valid_samples;
    }

    if (ctx->transmitted_samples == 0u && ctx->transmitted_samples + samples_to_send == ctx->requested_samples) {
      md = only_md;
    } else if (ctx->transmitted_samples == 0u) {
      md = start_md;
    } else if (ctx->transmitted_samples + samples_to_send == ctx->requested_samples) {
      md = end_md;
    }
    for (channel_index = 0u; channel_index < ctx->channel_count; ++channel_index) {
      buffer_ptrs[channel_index] =
          sc16_prefetch + (((size_t)channel_index * prefetch_capacity_samples + prefetch_offset_samples) * 2u);
    }
    if (uhd_tx_streamer_send(ctx->tx_streamer,
                             buffer_ptrs,
                             samples_to_send,
                             &md,
                             1.0,
                             &samples_sent) != UHD_ERROR_NONE) {
      (void)snprintf(ctx->error_message, sizeof(ctx->error_message), "%s", "failed while transmitting TRX TX samples");
      goto cleanup;
    }
    if (samples_sent != samples_to_send) {
      (void)snprintf(ctx->error_message, sizeof(ctx->error_message), "%s", "partial TX send in TRX mode");
      goto cleanup;
    }
    ctx->transmitted_samples += samples_sent;
    prefetch_offset_samples += samples_sent;
    prefetch_valid_samples -= samples_sent;
  }
  ctx->tx_prefetch_samples = prefetch_capacity_samples;

  {
    unsigned int attempts = 0u;

    for (attempts = 0u; attempts < 16u; ++attempts) {
      bool valid = false;
      uhd_async_metadata_event_code_t event_code = 0;

      if (uhd_tx_streamer_recv_async_msg(ctx->tx_streamer, &async_md, 0.1, &valid) != UHD_ERROR_NONE) {
        (void)snprintf(ctx->error_message, sizeof(ctx->error_message), "%s", "failed while reading TRX TX async metadata");
        goto cleanup;
      }
      if (!valid) {
        break;
      }
      if (uhd_async_metadata_event_code(async_md, &event_code) != UHD_ERROR_NONE) {
        (void)snprintf(ctx->error_message, sizeof(ctx->error_message), "%s", "failed to decode TRX TX async metadata");
        goto cleanup;
      }
      if (event_code == UHD_ASYNC_METADATA_EVENT_CODE_BURST_ACK) {
        ctx->burst_ack_valid = true;
        ctx->burst_ack = true;
      } else if (event_code == UHD_ASYNC_METADATA_EVENT_CODE_UNDERFLOW ||
                 event_code == UHD_ASYNC_METADATA_EVENT_CODE_UNDERFLOW_IN_PACKET) {
        ctx->underflow_observed = true;
      } else if (event_code == UHD_ASYNC_METADATA_EVENT_CODE_SEQ_ERROR ||
                 event_code == UHD_ASYNC_METADATA_EVENT_CODE_SEQ_ERROR_IN_BURST) {
        ctx->seq_error_observed = true;
      } else if (event_code == UHD_ASYNC_METADATA_EVENT_CODE_TIME_ERROR) {
        ctx->time_error_observed = true;
      }
    }
  }
  ctx->rc = 0;

cleanup:
  if (sc16_prefetch != NULL) {
    free(sc16_prefetch);
  }
  if (end_md != NULL) {
    uhd_tx_metadata_free(&end_md);
  }
  if (middle_md != NULL) {
    uhd_tx_metadata_free(&middle_md);
  }
  if (start_md != NULL) {
    uhd_tx_metadata_free(&start_md);
  }
  if (only_md != NULL) {
    uhd_tx_metadata_free(&only_md);
  }
  if (async_md != NULL) {
    uhd_async_metadata_free(&async_md);
  }
  return NULL;
}

int mini_gnb_c_b210_probe_run(const mini_gnb_c_b210_probe_config_t* config,
                              mini_gnb_c_b210_probe_report_t* report,
                              char* error_message,
                              const size_t error_message_size) {
  uhd_usrp_handle usrp = NULL;
  uhd_subdev_spec_handle subdev_spec = NULL;
  uhd_rx_streamer_handle rx_streamer = NULL;
  uhd_rx_metadata_handle md = NULL;
  uhd_stream_args_t stream_args;
  uhd_stream_cmd_t stream_cmd;
  size_t requested_samples = 0u;
  size_t samples_per_buffer = 0u;
  size_t channel_list[MINI_GNB_C_B210_MAX_CHANNELS];
  float* fc32_buffer[MINI_GNB_C_B210_MAX_CHANNELS] = {NULL, NULL};
  int16_t* sc16_buffer[MINI_GNB_C_B210_MAX_CHANNELS] = {NULL, NULL};
  int16_t* ring_staging = NULL;
  size_t ring_staging_samples = 0u;
  uint64_t ring_staging_hw_time_ns = 0u;
  uint32_t ring_staging_flags = 0u;
  void* buffer_ptrs[MINI_GNB_C_B210_MAX_CHANNELS] = {NULL, NULL};
  FILE* fp = NULL;
  mini_gnb_c_sc16_ring_map_t ring;
  mini_gnb_c_sc16_ring_map_config_t ring_config;
  size_t received_samples = 0u;
  uint64_t deadline_ns = 0u;
  uint64_t wall_start_ns = 0u;
  uint64_t prev_expected_next_hw_time_ns = 0u;
  int rc = -1;
  const bool use_ring_map = config->ring_path[0] != '\0';
  uint32_t channel_index = 0u;

  if (config == NULL || report == NULL) {
    return mini_gnb_c_b210_probe_fail(error_message, error_message_size, "invalid B210 probe arguments");
  }
  memset(report, 0, sizeof(*report));
  memset(&ring, 0, sizeof(ring));
  ring.fd = -1;
  mini_gnb_c_sc16_ring_map_config_init(&ring_config);
  report->channel_count = config->channel_count;
  if (config->rate_sps <= 0.0 || config->duration_sec <= 0.0 ||
      (!use_ring_map && config->output_path[0] == '\0')) {
    return mini_gnb_c_b210_probe_fail(error_message, error_message_size, "invalid B210 probe configuration");
  }
  if (config->channel_count == 0u || config->channel_count > MINI_GNB_C_B210_MAX_CHANNELS) {
    return mini_gnb_c_b210_probe_fail(error_message, error_message_size, "unsupported RX channel count");
  }
  if (!use_ring_map && config->channel_count != 1u) {
    return mini_gnb_c_b210_probe_fail(error_message,
                                      error_message_size,
                                      "raw RX file output currently supports only one channel; use --ring-map for multi-channel capture");
  }
  if (use_ring_map && (config->ring_block_samples == 0u || config->ring_block_count == 0u)) {
    return mini_gnb_c_b210_probe_fail(error_message, error_message_size, "invalid RX ring-map geometry");
  }
  requested_samples = (size_t)llround(config->rate_sps * config->duration_sec);
  if (requested_samples == 0u) {
    return mini_gnb_c_b210_probe_fail(error_message, error_message_size, "requested sample count is zero");
  }
  if (config->apply_host_tuning) {
    if (mini_gnb_c_radio_host_performance_prepare_for_backend(MINI_GNB_C_RADIO_BACKEND_B210,
                                                              report->host_tuning_summary,
                                                              sizeof(report->host_tuning_summary)) != 0) {
      return mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to prepare host performance tuning");
    }
  } else {
    (void)snprintf(report->host_tuning_summary, sizeof(report->host_tuning_summary), "%s", "host_tuning=disabled");
  }
  if (mini_gnb_c_b210_probe_pin_current_thread(config->cpu_core, error_message, error_message_size) != 0) {
    return -1;
  }

  if (uhd_usrp_make(&usrp, config->device_args) != UHD_ERROR_NONE) {
    return mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to create B210 device");
  }
  if (config->ref[0] != '\0' && uhd_usrp_set_clock_source(usrp, config->ref, 0u) != UHD_ERROR_NONE) {
    rc = mini_gnb_c_b210_probe_fill_uhd_error(usrp, "failed to set clock source", error_message, error_message_size);
    goto cleanup;
  }
  if (config->subdev[0] != '\0') {
    if (uhd_subdev_spec_make(&subdev_spec, config->subdev) != UHD_ERROR_NONE) {
      rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to create RX subdevice spec");
      goto cleanup;
    }
    if (uhd_usrp_set_rx_subdev_spec(usrp, subdev_spec, 0u) != UHD_ERROR_NONE) {
      rc = mini_gnb_c_b210_probe_fill_uhd_error(usrp, "failed to set RX subdevice", error_message, error_message_size);
      goto cleanup;
    }
  }
  if (mini_gnb_c_b210_probe_apply_rx_profile(usrp,
                                             config->channel,
                                             config->channel_count,
                                             config->rate_sps,
                                             config->gain_db,
                                             config->bandwidth_hz,
                                             config->freq_hz,
                                             error_message,
                                             error_message_size) != 0) {
    rc = -1;
    goto cleanup;
  }

  (void)uhd_usrp_get_pp_string(usrp, report->device_summary, sizeof(report->device_summary));
  (void)uhd_usrp_get_rx_subdev_name(usrp, config->channel, report->rx_subdev_name, sizeof(report->rx_subdev_name));
  (void)uhd_usrp_get_rx_rate(usrp, config->channel, &report->actual_rate_sps);
  (void)uhd_usrp_get_rx_freq(usrp, config->channel, &report->actual_freq_hz);
  (void)uhd_usrp_get_rx_gain(usrp, config->channel, "", &report->actual_gain_db);
  (void)uhd_usrp_get_rx_bandwidth(usrp, config->channel, &report->actual_bandwidth_hz);

  if (mini_gnb_c_b210_probe_optional_sensor_bool(usrp,
                                                 true,
                                                 "ref_locked",
                                                 0u,
                                                 &report->ref_locked_valid,
                                                 &report->ref_locked,
                                                 NULL,
                                                 0u) != 0) {
    report->ref_locked_valid = false;
  }
  mini_gnb_c_b210_probe_collect_rx_lo_lock(usrp,
                                           config->channel,
                                           config->channel_count,
                                           &report->lo_locked_valid,
                                           &report->lo_locked);
  if (config->require_ref_lock && report->ref_locked_valid && !report->ref_locked) {
    rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "reference clock is not locked");
    goto cleanup;
  }
  if (config->require_lo_lock && report->lo_locked_valid && !report->lo_locked) {
    rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "RX LO is not locked");
    goto cleanup;
  }

  if (uhd_rx_streamer_make(&rx_streamer) != UHD_ERROR_NONE || uhd_rx_metadata_make(&md) != UHD_ERROR_NONE) {
    rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to create UHD RX streaming objects");
    goto cleanup;
  }

  memset(&stream_args, 0, sizeof(stream_args));
  if (mini_gnb_c_b210_probe_fill_channel_list(config->channel,
                                              config->channel_count,
                                              channel_list,
                                              error_message,
                                              error_message_size) != 0) {
    rc = -1;
    goto cleanup;
  }
  stream_args.cpu_format = use_ring_map ? "sc16" : "fc32";
  stream_args.otw_format = "sc16";
  stream_args.args = "";
  stream_args.channel_list = channel_list;
  stream_args.n_channels = config->channel_count;
  if (uhd_usrp_get_rx_stream(usrp, &stream_args, rx_streamer) != UHD_ERROR_NONE ||
      uhd_rx_streamer_max_num_samps(rx_streamer, &samples_per_buffer) != UHD_ERROR_NONE ||
      samples_per_buffer == 0u) {
    rc = mini_gnb_c_b210_probe_fill_uhd_error(usrp, "failed to create UHD RX stream", error_message, error_message_size);
    goto cleanup;
  }

  if (use_ring_map) {
    ring_config.role = MINI_GNB_C_SC16_RING_ROLE_RX;
    ring_config.channel_count = config->channel_count;
    ring_config.block_count = config->ring_block_count;
    ring_config.block_samples = config->ring_block_samples;
    ring_config.sample_rate_sps = (uint64_t)llround(config->rate_sps);
    for (channel_index = 0u; channel_index < config->channel_count; ++channel_index) {
      sc16_buffer[channel_index] = (int16_t*)malloc(samples_per_buffer * 2u * sizeof(int16_t));
      if (sc16_buffer[channel_index] == NULL) {
        rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to allocate RX sc16 sample buffer");
        goto cleanup;
      }
      buffer_ptrs[channel_index] = sc16_buffer[channel_index];
    }
    ring_staging =
        (int16_t*)malloc((size_t)config->ring_block_samples * config->channel_count * 2u * sizeof(int16_t));
    if (ring_staging == NULL) {
      rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to allocate RX ring staging buffer");
      goto cleanup;
    }
    if (mini_gnb_c_sc16_ring_map_create(config->ring_path, &ring_config, &ring, error_message, error_message_size) != 0) {
      rc = -1;
      goto cleanup;
    }
  } else {
    fc32_buffer[0] = (float*)malloc(samples_per_buffer * 2u * sizeof(float));
    if (fc32_buffer[0] == NULL) {
      rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to allocate RX sample buffer");
      goto cleanup;
    }
    buffer_ptrs[0] = fc32_buffer[0];

    fp = fopen(config->output_path, "wb");
    if (fp == NULL) {
      rc = mini_gnb_c_b210_probe_failf(error_message, error_message_size, "failed to open output file", config->output_path);
      goto cleanup;
    }
  }

  if (mini_gnb_c_b210_probe_issue_rx_start(usrp,
                                           rx_streamer,
                                           config->channel_count,
                                           error_message,
                                           error_message_size) != 0) {
    rc = -1;
    goto cleanup;
  }
  wall_start_ns = mini_gnb_c_b210_probe_now_monotonic_ns();
  if (config->duration_mode == MINI_GNB_C_B210_DURATION_MODE_WALLCLOCK && wall_start_ns > 0u) {
    deadline_ns = wall_start_ns + (uint64_t)(config->duration_sec * 1000000000.0);
  }

  while (!mini_gnb_c_b210_duration_deadline_reached(config->duration_mode, deadline_ns) &&
         (config->duration_mode == MINI_GNB_C_B210_DURATION_MODE_WALLCLOCK || received_samples < requested_samples)) {
    size_t got = 0u;
    size_t samples_to_write = 0u;
    uhd_rx_metadata_error_code_t error_code = UHD_RX_METADATA_ERROR_CODE_NONE;
    const double recv_timeout_sec =
        mini_gnb_c_b210_compute_rx_timeout_sec(config->duration_mode, deadline_ns, 3.0);

    if (uhd_rx_streamer_recv(rx_streamer,
                             buffer_ptrs,
                             samples_per_buffer,
                             &md,
                             recv_timeout_sec,
                             false,
                             &got) != UHD_ERROR_NONE) {
      rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed while receiving RX samples");
      goto cleanup;
    }
    if (uhd_rx_metadata_error_code(md, &error_code) != UHD_ERROR_NONE) {
      rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to read RX metadata");
      goto cleanup;
    }
    if (error_code != UHD_RX_METADATA_ERROR_CODE_NONE) {
      if (mini_gnb_c_b210_is_recoverable_rx_metadata_error(error_code)) {
        if (error_code == UHD_RX_METADATA_ERROR_CODE_OVERFLOW) {
          report->rx_overflow_events += 1u;
        } else if (error_code == UHD_RX_METADATA_ERROR_CODE_TIMEOUT) {
          report->rx_timeout_events += 1u;
        }
        report->rx_recoverable_events += 1u;
        continue;
      } else {
        char detail[192];

        if (error_code == UHD_RX_METADATA_ERROR_CODE_OVERFLOW) {
          (void)snprintf(detail,
                         sizeof(detail),
                         "UHD RX overflow (0x%x); pin the RX thread and capture to tmpfs such as /dev/shm",
                         (unsigned)error_code);
        } else {
          (void)snprintf(detail, sizeof(detail), "UHD RX metadata error code 0x%x", (unsigned)error_code);
        }
        rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, detail);
        goto cleanup;
      }
    }
    if (got == 0u) {
      continue;
    }
    if (config->duration_mode == MINI_GNB_C_B210_DURATION_MODE_WALLCLOCK) {
      samples_to_write = got;
    } else {
      const size_t remaining = requested_samples - received_samples;

      samples_to_write = got > remaining ? remaining : got;
    }
    if (use_ring_map) {
      uint64_t recv_hw_time_ns = 0u;
      bool recv_hw_time_valid = false;
      size_t offset_samples = 0u;

      if (mini_gnb_c_b210_probe_rx_metadata_time_ns(md,
                                                    &recv_hw_time_ns,
                                                    &recv_hw_time_valid,
                                                    error_message,
                                                    error_message_size) != 0) {
        rc = -1;
        goto cleanup;
      }
      if (recv_hw_time_valid) {
        const size_t lost_samples =
            mini_gnb_c_b210_estimate_lost_samples_from_gap(prev_expected_next_hw_time_ns, recv_hw_time_ns, config->rate_sps);

        if (lost_samples > 0u) {
          report->rx_gap_events += 1u;
          report->rx_lost_samples_estimate += lost_samples;
        }
      }

      while (offset_samples < samples_to_write) {
        const size_t free_samples = config->ring_block_samples - ring_staging_samples;
        const size_t copy_samples = (samples_to_write - offset_samples) > free_samples ? free_samples : (samples_to_write - offset_samples);

        if (ring_staging_samples == 0u) {
          if (recv_hw_time_valid) {
            ring_staging_hw_time_ns =
                mini_gnb_c_b210_advance_time_ns(recv_hw_time_ns, offset_samples, config->rate_sps);
            ring_staging_flags = MINI_GNB_C_SC16_RING_FLAG_HW_TIME_VALID;
          } else {
            ring_staging_hw_time_ns = mini_gnb_c_b210_probe_now_monotonic_ns();
            ring_staging_flags = MINI_GNB_C_SC16_RING_FLAG_HOST_TIME_FALLBACK;
          }
        }

        for (channel_index = 0u; channel_index < config->channel_count; ++channel_index) {
          memcpy(ring_staging + (((size_t)channel_index * config->ring_block_samples + ring_staging_samples) * 2u),
                 sc16_buffer[channel_index] + (offset_samples * 2u),
                 copy_samples * 2u * sizeof(int16_t));
        }
        ring_staging_samples += copy_samples;
        offset_samples += copy_samples;

        if (ring_staging_samples == config->ring_block_samples) {
          if (mini_gnb_c_sc16_ring_map_append(&ring,
                                              ring_staging,
                                              (uint32_t)ring_staging_samples,
                                              ring_staging_hw_time_ns,
                                              ring_staging_flags,
                                              error_message,
                                              error_message_size) != 0) {
            rc = -1;
            goto cleanup;
          }
          report->ring_blocks_committed += 1u;
          ring_staging_samples = 0u;
          ring_staging_hw_time_ns = 0u;
          ring_staging_flags = 0u;
        }
      }
      if (recv_hw_time_valid) {
        prev_expected_next_hw_time_ns =
            mini_gnb_c_b210_advance_time_ns(recv_hw_time_ns, samples_to_write, config->rate_sps);
      }
    } else {
      if (fwrite(fc32_buffer[0], sizeof(float) * 2u, samples_to_write, fp) != samples_to_write) {
        rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to write RX samples to file");
        goto cleanup;
      }
    }
    received_samples += samples_to_write;
  }
  if (use_ring_map && ring_staging_samples > 0u) {
    if (mini_gnb_c_sc16_ring_map_append(&ring,
                                        ring_staging,
                                        (uint32_t)ring_staging_samples,
                                        ring_staging_hw_time_ns,
                                        ring_staging_flags,
                                        error_message,
                                        error_message_size) != 0) {
      rc = -1;
      goto cleanup;
    }
    report->ring_blocks_committed += 1u;
  }

  report->requested_samples = requested_samples;
  report->received_samples = received_samples;
  report->used_ring_map = use_ring_map;
  report->duration_mode = config->duration_mode;
  if (wall_start_ns > 0u) {
    const uint64_t wall_end_ns = mini_gnb_c_b210_probe_now_monotonic_ns();

    if (wall_end_ns >= wall_start_ns) {
      report->wall_elapsed_sec = (double)(wall_end_ns - wall_start_ns) / 1000000000.0;
    }
  }
  rc = 0;

cleanup:
  if (rx_streamer != NULL) {
    memset(&stream_cmd, 0, sizeof(stream_cmd));
    stream_cmd.stream_mode = UHD_STREAM_MODE_STOP_CONTINUOUS;
    stream_cmd.stream_now = false;
    (void)uhd_rx_streamer_issue_stream_cmd(rx_streamer, &stream_cmd);
  }
  mini_gnb_c_sc16_ring_map_close(&ring);
  if (fp != NULL) {
    fclose(fp);
    fp = NULL;
  }
  for (channel_index = 0u; channel_index < MINI_GNB_C_B210_MAX_CHANNELS; ++channel_index) {
    if (fc32_buffer[channel_index] != NULL) {
      free(fc32_buffer[channel_index]);
      fc32_buffer[channel_index] = NULL;
    }
    if (sc16_buffer[channel_index] != NULL) {
      free(sc16_buffer[channel_index]);
      sc16_buffer[channel_index] = NULL;
    }
  }
  if (ring_staging != NULL) {
    free(ring_staging);
    ring_staging = NULL;
  }
  if (md != NULL) {
    uhd_rx_metadata_free(&md);
  }
  if (rx_streamer != NULL) {
    uhd_rx_streamer_free(&rx_streamer);
  }
  if (subdev_spec != NULL) {
    uhd_subdev_spec_free(&subdev_spec);
  }
  if (usrp != NULL) {
    uhd_usrp_free(&usrp);
  }
  return rc;
}

int mini_gnb_c_b210_tx_from_file_run(const mini_gnb_c_b210_tx_config_t* config,
                                     mini_gnb_c_b210_tx_report_t* report,
                                     char* error_message,
                                     const size_t error_message_size) {
  uhd_usrp_handle usrp = NULL;
  uhd_subdev_spec_handle subdev_spec = NULL;
  uhd_tx_streamer_handle tx_streamer = NULL;
  uhd_async_metadata_handle async_md = NULL;
  uhd_tx_metadata_handle only_md = NULL;
  uhd_tx_metadata_handle start_md = NULL;
  uhd_tx_metadata_handle middle_md = NULL;
  uhd_tx_metadata_handle end_md = NULL;
  uhd_stream_args_t stream_args;
  size_t channel_list[MINI_GNB_C_B210_MAX_CHANNELS];
  size_t samples_per_buffer = 0u;
  struct stat st;
  FILE* fp = NULL;
  float* input_samples = NULL;
  int16_t* sc16_prefetch = NULL;
  const void* buffer_ptrs[MINI_GNB_C_B210_MAX_CHANNELS] = {NULL, NULL};
  mini_gnb_c_sc16_ring_map_t ring;
  size_t requested_samples = 0u;
  size_t transmitted_samples = 0u;
  size_t ring_blocks_consumed = 0u;
  int rc = -1;
  const bool use_ring_map = config->ring_path[0] != '\0';
  uint64_t prefetch_seq = 0u;
  size_t prefetch_block_offset_samples = 0u;
  size_t prefetch_capacity_samples = 0u;
  size_t prefetch_valid_samples = 0u;
  size_t prefetch_offset_samples = 0u;
  const size_t prefetch_low_watermark_divisor = 2u;

  if (config == NULL || report == NULL) {
    return mini_gnb_c_b210_probe_fail(error_message, error_message_size, "invalid B210 TX arguments");
  }
  memset(report, 0, sizeof(*report));
  memset(&ring, 0, sizeof(ring));
  ring.fd = -1;
  report->channel_count = config->channel_count;
  if (config->rate_sps <= 0.0 || (!use_ring_map && config->input_path[0] == '\0')) {
    return mini_gnb_c_b210_probe_fail(error_message, error_message_size, "invalid B210 TX configuration");
  }
  if (config->channel_count == 0u || config->channel_count > MINI_GNB_C_B210_MAX_CHANNELS) {
    return mini_gnb_c_b210_probe_fail(error_message, error_message_size, "unsupported TX channel count");
  }
  if (!use_ring_map && config->channel_count != 1u) {
    return mini_gnb_c_b210_probe_fail(error_message,
                                      error_message_size,
                                      "raw TX file input currently supports only one channel; use --ring-map for multi-channel replay");
  }
  if (config->apply_host_tuning) {
    if (mini_gnb_c_radio_host_performance_prepare_for_backend(MINI_GNB_C_RADIO_BACKEND_B210,
                                                              report->host_tuning_summary,
                                      sizeof(report->host_tuning_summary)) != 0) {
      return mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to prepare host performance tuning");
    }
  } else {
    (void)snprintf(report->host_tuning_summary, sizeof(report->host_tuning_summary), "%s", "host_tuning=disabled");
  }
  if (use_ring_map) {
    uint64_t seq = 0u;

    if (mini_gnb_c_sc16_ring_map_open_existing(config->ring_path, false, &ring, error_message, error_message_size) != 0) {
      rc = -1;
      goto cleanup;
    }
    if (ring.superblock->channel_count != config->channel_count) {
      rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "TX ring-map channel count does not match probe channel count");
      goto cleanup;
    }
    if (ring.superblock->last_committed_seq == UINT64_MAX || ring.superblock->next_write_seq == 0u) {
      rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "TX ring map does not contain ready blocks");
      goto cleanup;
    }
    for (seq = ring.superblock->oldest_valid_seq; seq < ring.superblock->next_write_seq; ++seq) {
      const mini_gnb_c_sc16_ring_map_descriptor_t* descriptor = mini_gnb_c_sc16_ring_map_get_descriptor(&ring, seq);

      if (descriptor != NULL) {
        requested_samples += descriptor->sample_count;
        report->ring_blocks_committed += 1u;
      }
    }
    if (requested_samples == 0u) {
      rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "TX ring map has no readable samples");
      goto cleanup;
    }
    prefetch_seq = ring.superblock->oldest_valid_seq;
  } else {
    if (stat(config->input_path, &st) != 0) {
      return mini_gnb_c_b210_probe_failf(error_message, error_message_size, "failed to stat TX input file", config->input_path);
    }
    if (st.st_size <= 0 || (st.st_size % (off_t)(sizeof(float) * 2u)) != 0) {
      return mini_gnb_c_b210_probe_fail(error_message,
                                        error_message_size,
                                        "TX input file size must be a positive multiple of one fc32 sample");
    }
    requested_samples = (size_t)(st.st_size / (off_t)(sizeof(float) * 2u));
    if (requested_samples == 0u) {
      return mini_gnb_c_b210_probe_fail(error_message, error_message_size, "TX input file contains zero samples");
    }
    fp = fopen(config->input_path, "rb");
    if (fp == NULL) {
      return mini_gnb_c_b210_probe_failf(error_message, error_message_size, "failed to open TX input file", config->input_path);
    }
    input_samples = (float*)malloc((size_t)st.st_size);
    if (input_samples == NULL) {
      rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to allocate TX input sample buffer");
      goto cleanup;
    }
    if (fread(input_samples, 1u, (size_t)st.st_size, fp) != (size_t)st.st_size) {
      rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to preload TX samples into memory");
      goto cleanup;
    }
    fclose(fp);
    fp = NULL;
  }

  if (uhd_usrp_make(&usrp, config->device_args) != UHD_ERROR_NONE) {
    rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to create B210 device");
    goto cleanup;
  }
  if (config->ref[0] != '\0' && uhd_usrp_set_clock_source(usrp, config->ref, 0u) != UHD_ERROR_NONE) {
    rc = mini_gnb_c_b210_probe_fill_uhd_error(usrp, "failed to set clock source", error_message, error_message_size);
    goto cleanup;
  }
  if (config->subdev[0] != '\0') {
    if (uhd_subdev_spec_make(&subdev_spec, config->subdev) != UHD_ERROR_NONE) {
      rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to create TX subdevice spec");
      goto cleanup;
    }
    if (uhd_usrp_set_tx_subdev_spec(usrp, subdev_spec, 0u) != UHD_ERROR_NONE) {
      rc = mini_gnb_c_b210_probe_fill_uhd_error(usrp, "failed to set TX subdevice", error_message, error_message_size);
      goto cleanup;
    }
  }
  if (mini_gnb_c_b210_probe_apply_tx_profile(usrp,
                                             config->channel,
                                             config->channel_count,
                                             config->rate_sps,
                                             config->gain_db,
                                             config->bandwidth_hz,
                                             config->freq_hz,
                                             error_message,
                                             error_message_size) != 0) {
    rc = -1;
    goto cleanup;
  }

  (void)uhd_usrp_get_pp_string(usrp, report->device_summary, sizeof(report->device_summary));
  (void)uhd_usrp_get_tx_subdev_name(usrp, config->channel, report->tx_subdev_name, sizeof(report->tx_subdev_name));
  (void)uhd_usrp_get_tx_rate(usrp, config->channel, &report->actual_rate_sps);
  (void)uhd_usrp_get_tx_freq(usrp, config->channel, &report->actual_freq_hz);
  (void)uhd_usrp_get_tx_gain(usrp, config->channel, "", &report->actual_gain_db);
  (void)uhd_usrp_get_tx_bandwidth(usrp, config->channel, &report->actual_bandwidth_hz);

  if (mini_gnb_c_b210_probe_optional_sensor_bool(usrp,
                                                 true,
                                                 "ref_locked",
                                                 0u,
                                                 &report->ref_locked_valid,
                                                 &report->ref_locked,
                                                 NULL,
                                                 0u) != 0) {
    report->ref_locked_valid = false;
  }
  mini_gnb_c_b210_probe_collect_tx_lo_lock(usrp,
                                           config->channel,
                                           config->channel_count,
                                           &report->lo_locked_valid,
                                           &report->lo_locked);
  if (config->require_ref_lock && report->ref_locked_valid && !report->ref_locked) {
    rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "reference clock is not locked");
    goto cleanup;
  }
  if (config->require_lo_lock && report->lo_locked_valid && !report->lo_locked) {
    rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "TX LO is not locked");
    goto cleanup;
  }

  if (uhd_tx_streamer_make(&tx_streamer) != UHD_ERROR_NONE) {
    rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to create UHD TX streaming objects");
    goto cleanup;
  }

  memset(&stream_args, 0, sizeof(stream_args));
  if (mini_gnb_c_b210_probe_fill_channel_list(config->channel,
                                              config->channel_count,
                                              channel_list,
                                              error_message,
                                              error_message_size) != 0) {
    rc = -1;
    goto cleanup;
  }
  stream_args.cpu_format = use_ring_map ? "sc16" : "fc32";
  stream_args.otw_format = "sc16";
  stream_args.args = "";
  stream_args.channel_list = channel_list;
  stream_args.n_channels = config->channel_count;
  if (uhd_usrp_get_tx_stream(usrp, &stream_args, tx_streamer) != UHD_ERROR_NONE ||
      uhd_tx_streamer_max_num_samps(tx_streamer, &samples_per_buffer) != UHD_ERROR_NONE ||
      samples_per_buffer == 0u) {
    rc = mini_gnb_c_b210_probe_fill_tx_stream_error(tx_streamer, "failed to create UHD TX stream", error_message, error_message_size);
    goto cleanup;
  }

  if (mini_gnb_c_b210_probe_pin_current_thread(config->cpu_core, error_message, error_message_size) != 0) {
    rc = -1;
    goto cleanup;
  }
  if (uhd_async_metadata_make(&async_md) != UHD_ERROR_NONE ||
      uhd_tx_metadata_make(&only_md, false, 0, 0.0, true, true) != UHD_ERROR_NONE ||
      uhd_tx_metadata_make(&start_md, false, 0, 0.0, true, false) != UHD_ERROR_NONE ||
      uhd_tx_metadata_make(&middle_md, false, 0, 0.0, false, false) != UHD_ERROR_NONE ||
      uhd_tx_metadata_make(&end_md, false, 0, 0.0, false, true) != UHD_ERROR_NONE) {
    rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to allocate TX metadata");
    goto cleanup;
  }

  if (use_ring_map) {
    const size_t prefetch_multiplier = 64u;

    if (config->tx_prefetch_samples > 0u) {
      prefetch_capacity_samples = config->tx_prefetch_samples;
    } else {
      prefetch_capacity_samples = samples_per_buffer * prefetch_multiplier;
    }
    if (prefetch_capacity_samples < ring.superblock->block_samples) {
      prefetch_capacity_samples = ring.superblock->block_samples;
    }
    if (prefetch_capacity_samples < samples_per_buffer) {
      prefetch_capacity_samples = samples_per_buffer;
    }
    if (prefetch_capacity_samples > requested_samples) {
      prefetch_capacity_samples = requested_samples;
    }
    if (prefetch_capacity_samples == 0u) {
      prefetch_capacity_samples = samples_per_buffer;
    }
    sc16_prefetch =
        (int16_t*)malloc(prefetch_capacity_samples * config->channel_count * 2u * sizeof(int16_t));
    if (sc16_prefetch == NULL) {
      rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to allocate TX prefetch buffer");
      goto cleanup;
    }
  }

  while (transmitted_samples < requested_samples) {
    size_t samples_to_send = 0u;
    size_t samples_sent = 0u;
    uhd_tx_metadata_handle md = middle_md;

    if (use_ring_map) {
      const size_t prefetch_low_watermark =
          prefetch_capacity_samples / prefetch_low_watermark_divisor;

      if (prefetch_valid_samples <= prefetch_low_watermark &&
          prefetch_seq < ring.superblock->next_write_seq) {
        uint32_t channel_index = 0u;

        if (prefetch_valid_samples > 0u && prefetch_offset_samples > 0u) {
          for (channel_index = 0u; channel_index < config->channel_count; ++channel_index) {
            int16_t* channel_base =
                sc16_prefetch + ((size_t)channel_index * prefetch_capacity_samples * 2u);

            memmove(channel_base,
                    channel_base + (prefetch_offset_samples * 2u),
                    prefetch_valid_samples * 2u * sizeof(int16_t));
          }
        }
        prefetch_offset_samples = 0u;
        while (prefetch_valid_samples < prefetch_capacity_samples &&
               prefetch_seq < ring.superblock->next_write_seq) {
          const mini_gnb_c_sc16_ring_map_descriptor_t* descriptor =
              mini_gnb_c_sc16_ring_map_get_descriptor(&ring, prefetch_seq);
          size_t available_in_block = 0u;
          size_t samples_to_copy = 0u;

          if (descriptor == NULL || descriptor->sample_count == 0u) {
            prefetch_seq += 1u;
            prefetch_block_offset_samples = 0u;
            continue;
          }
          if (prefetch_block_offset_samples >= descriptor->sample_count) {
            prefetch_seq += 1u;
            prefetch_block_offset_samples = 0u;
            continue;
          }
          available_in_block = descriptor->sample_count - prefetch_block_offset_samples;
          samples_to_copy = prefetch_capacity_samples - prefetch_valid_samples;
          if (samples_to_copy > available_in_block) {
            samples_to_copy = available_in_block;
          }
          for (channel_index = 0u; channel_index < config->channel_count; ++channel_index) {
            const int16_t* channel_payload =
                mini_gnb_c_sc16_ring_map_get_channel_payload(&ring, prefetch_seq, channel_index);
            int16_t* staging_channel =
                sc16_prefetch + (((size_t)channel_index * prefetch_capacity_samples + prefetch_valid_samples) * 2u);

            if (channel_payload == NULL) {
              rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to resolve TX ring payload");
              goto cleanup;
            }
            memcpy(staging_channel,
                   channel_payload + (prefetch_block_offset_samples * 2u),
                   samples_to_copy * 2u * sizeof(int16_t));
          }
          prefetch_valid_samples += samples_to_copy;
          prefetch_block_offset_samples += samples_to_copy;
          if (prefetch_block_offset_samples == descriptor->sample_count) {
            prefetch_seq += 1u;
            prefetch_block_offset_samples = 0u;
            ring_blocks_consumed += 1u;
          }
        }
      }

      if (prefetch_valid_samples == 0u) {
        rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "TX prefetch buffer ran empty");
        goto cleanup;
      }
      samples_to_send = requested_samples - transmitted_samples;
      if (samples_to_send > samples_per_buffer) {
        samples_to_send = samples_per_buffer;
      }
      if (samples_to_send > prefetch_valid_samples) {
        samples_to_send = prefetch_valid_samples;
      }
      {
        uint32_t channel_index = 0u;

        for (channel_index = 0u; channel_index < config->channel_count; ++channel_index) {
          buffer_ptrs[channel_index] =
              sc16_prefetch + (((size_t)channel_index * prefetch_capacity_samples + prefetch_offset_samples) * 2u);
        }
      }
    } else {
      samples_to_send = requested_samples - transmitted_samples;
      if (samples_to_send > samples_per_buffer) {
        samples_to_send = samples_per_buffer;
      }
      buffer_ptrs[0] = input_samples + (transmitted_samples * 2u);
    }

    if (transmitted_samples == 0u && transmitted_samples + samples_to_send == requested_samples) {
      md = only_md;
    } else if (transmitted_samples == 0u) {
      md = start_md;
    } else if (transmitted_samples + samples_to_send == requested_samples) {
      md = end_md;
    }

    if (uhd_tx_streamer_send(tx_streamer, buffer_ptrs, samples_to_send, &md, 1.0, &samples_sent) !=
        UHD_ERROR_NONE) {
      rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed while transmitting TX samples");
      goto cleanup;
    }
    if (samples_sent != samples_to_send) {
      rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "partial TX send while transmitting samples");
      goto cleanup;
    }

    transmitted_samples += samples_sent;
    if (use_ring_map) {
      prefetch_offset_samples += samples_sent;
      prefetch_valid_samples -= samples_sent;
    }
  }

  {
    unsigned int attempts = 0u;

    for (attempts = 0u; attempts < 16u; ++attempts) {
      bool valid = false;
      uhd_async_metadata_event_code_t event_code = 0;

      if (uhd_tx_streamer_recv_async_msg(tx_streamer, &async_md, 0.1, &valid) != UHD_ERROR_NONE) {
        rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed while reading TX async metadata");
        goto cleanup;
      }
      if (!valid) {
        break;
      }
      if (uhd_async_metadata_event_code(async_md, &event_code) != UHD_ERROR_NONE) {
        rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to decode TX async metadata");
        goto cleanup;
      }
      if (event_code == UHD_ASYNC_METADATA_EVENT_CODE_BURST_ACK) {
        report->burst_ack_valid = true;
        report->burst_ack = true;
      } else if (event_code == UHD_ASYNC_METADATA_EVENT_CODE_UNDERFLOW ||
                 event_code == UHD_ASYNC_METADATA_EVENT_CODE_UNDERFLOW_IN_PACKET) {
        report->underflow_observed = true;
      } else if (event_code == UHD_ASYNC_METADATA_EVENT_CODE_SEQ_ERROR ||
                 event_code == UHD_ASYNC_METADATA_EVENT_CODE_SEQ_ERROR_IN_BURST) {
        report->seq_error_observed = true;
      } else if (event_code == UHD_ASYNC_METADATA_EVENT_CODE_TIME_ERROR) {
        report->time_error_observed = true;
      }
    }
  }

  report->requested_samples = requested_samples;
  report->transmitted_samples = transmitted_samples;
  report->used_ring_map = use_ring_map;
  report->ring_blocks_committed = use_ring_map ? ring_blocks_consumed : 0u;
  report->tx_prefetch_samples = use_ring_map ? prefetch_capacity_samples : 0u;
  rc = 0;

cleanup:
  mini_gnb_c_sc16_ring_map_close(&ring);
  if (end_md != NULL) {
    uhd_tx_metadata_free(&end_md);
  }
  if (middle_md != NULL) {
    uhd_tx_metadata_free(&middle_md);
  }
  if (start_md != NULL) {
    uhd_tx_metadata_free(&start_md);
  }
  if (only_md != NULL) {
    uhd_tx_metadata_free(&only_md);
  }
  if (async_md != NULL) {
    uhd_async_metadata_free(&async_md);
  }
  if (sc16_prefetch != NULL) {
    free(sc16_prefetch);
    sc16_prefetch = NULL;
  }
  if (input_samples != NULL) {
    free(input_samples);
    input_samples = NULL;
  }
  if (fp != NULL) {
    fclose(fp);
    fp = NULL;
  }
  if (tx_streamer != NULL) {
    uhd_tx_streamer_free(&tx_streamer);
  }
  if (subdev_spec != NULL) {
    uhd_subdev_spec_free(&subdev_spec);
  }
  if (usrp != NULL) {
    uhd_usrp_free(&usrp);
  }
  return rc;
}

int mini_gnb_c_b210_trx_run(const mini_gnb_c_b210_trx_config_t* config,
                            mini_gnb_c_b210_trx_report_t* report,
                            char* error_message,
                            const size_t error_message_size) {
  uhd_usrp_handle usrp = NULL;
  uhd_subdev_spec_handle rx_subdev_spec = NULL;
  uhd_subdev_spec_handle tx_subdev_spec = NULL;
  uhd_rx_streamer_handle rx_streamer = NULL;
  uhd_tx_streamer_handle tx_streamer = NULL;
  uhd_stream_args_t rx_stream_args;
  uhd_stream_args_t tx_stream_args;
  uhd_stream_cmd_t stream_cmd;
  size_t channel_list[MINI_GNB_C_B210_MAX_CHANNELS];
  size_t rx_samples_per_buffer = 0u;
  size_t tx_samples_per_buffer = 0u;
  size_t requested_samples = 0u;
  size_t tx_ring_total_samples = 0u;
  uint64_t wall_start_ns = 0u;
  uint64_t deadline_ns = 0u;
  mini_gnb_c_sc16_ring_map_t rx_ring;
  mini_gnb_c_sc16_ring_map_t tx_ring;
  mini_gnb_c_sc16_ring_map_config_t rx_ring_config;
  mini_gnb_c_b210_trx_rx_worker_t rx_worker;
  mini_gnb_c_b210_trx_tx_worker_t tx_worker;
  pthread_t rx_thread;
  pthread_t tx_thread;
  bool rx_thread_started = false;
  bool tx_thread_started = false;
  int rc = -1;
  uint64_t seq = 0u;

  if (config == NULL || report == NULL) {
    return mini_gnb_c_b210_probe_fail(error_message, error_message_size, "invalid B210 TRX arguments");
  }
  memset(report, 0, sizeof(*report));
  memset(&rx_ring, 0, sizeof(rx_ring));
  memset(&tx_ring, 0, sizeof(tx_ring));
  memset(&rx_ring_config, 0, sizeof(rx_ring_config));
  memset(&rx_worker, 0, sizeof(rx_worker));
  memset(&tx_worker, 0, sizeof(tx_worker));
  rx_ring.fd = -1;
  tx_ring.fd = -1;
  mini_gnb_c_sc16_ring_map_config_init(&rx_ring_config);
  report->channel_count = config->channel_count;

  if (config->rate_sps <= 0.0 || config->channel_count == 0u || config->channel_count > MINI_GNB_C_B210_MAX_CHANNELS ||
      config->tx_ring_path[0] == '\0' || config->rx_ring_path[0] == '\0') {
    return mini_gnb_c_b210_probe_fail(error_message, error_message_size, "TRX mode currently requires valid RX and TX ring-map paths");
  }
  if (config->ring_block_samples == 0u || config->ring_block_count == 0u) {
    return mini_gnb_c_b210_probe_fail(error_message, error_message_size, "invalid TRX ring-map geometry");
  }
  if (config->apply_host_tuning) {
    if (mini_gnb_c_radio_host_performance_prepare_for_backend(MINI_GNB_C_RADIO_BACKEND_B210,
                                                              report->host_tuning_summary,
                                                              sizeof(report->host_tuning_summary)) != 0) {
      return mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to prepare host performance tuning");
    }
  } else {
    (void)snprintf(report->host_tuning_summary, sizeof(report->host_tuning_summary), "%s", "host_tuning=disabled");
  }
  if (mini_gnb_c_sc16_ring_map_open_existing(config->tx_ring_path, false, &tx_ring, error_message, error_message_size) != 0) {
    return -1;
  }
  if (tx_ring.superblock->channel_count != config->channel_count) {
    rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "TRX TX ring-map channel count does not match probe channel count");
    goto cleanup;
  }
  for (seq = tx_ring.superblock->oldest_valid_seq; seq < tx_ring.superblock->next_write_seq; ++seq) {
    const mini_gnb_c_sc16_ring_map_descriptor_t* descriptor = mini_gnb_c_sc16_ring_map_get_descriptor(&tx_ring, seq);

    if (descriptor != NULL) {
      tx_ring_total_samples += descriptor->sample_count;
    }
  }
  if (tx_ring_total_samples == 0u) {
    rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "TRX TX ring map contains no ready samples");
    goto cleanup;
  }
  if (config->duration_sec > 0.0) {
    requested_samples = (size_t)llround(config->rate_sps * config->duration_sec);
  }
  if (requested_samples == 0u) {
    requested_samples = tx_ring_total_samples;
  }

  rx_ring_config.role = MINI_GNB_C_SC16_RING_ROLE_RX;
  rx_ring_config.channel_count = config->channel_count;
  rx_ring_config.block_count = config->ring_block_count;
  rx_ring_config.block_samples = config->ring_block_samples;
  rx_ring_config.sample_rate_sps = (uint64_t)llround(config->rate_sps);
  if (mini_gnb_c_sc16_ring_map_create(config->rx_ring_path, &rx_ring_config, &rx_ring, error_message, error_message_size) != 0) {
    rc = -1;
    goto cleanup;
  }

  if (uhd_usrp_make(&usrp, config->device_args) != UHD_ERROR_NONE) {
    rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to create B210 device");
    goto cleanup;
  }
  if (config->ref[0] != '\0' && uhd_usrp_set_clock_source(usrp, config->ref, 0u) != UHD_ERROR_NONE) {
    rc = mini_gnb_c_b210_probe_fill_uhd_error(usrp, "failed to set clock source", error_message, error_message_size);
    goto cleanup;
  }
  if (config->subdev[0] != '\0') {
    if (uhd_subdev_spec_make(&rx_subdev_spec, config->subdev) != UHD_ERROR_NONE ||
        uhd_subdev_spec_make(&tx_subdev_spec, config->subdev) != UHD_ERROR_NONE) {
      rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to create TRX subdevice spec");
      goto cleanup;
    }
    if (uhd_usrp_set_rx_subdev_spec(usrp, rx_subdev_spec, 0u) != UHD_ERROR_NONE ||
        uhd_usrp_set_tx_subdev_spec(usrp, tx_subdev_spec, 0u) != UHD_ERROR_NONE) {
      rc = mini_gnb_c_b210_probe_fill_uhd_error(usrp, "failed to set TRX subdevice", error_message, error_message_size);
      goto cleanup;
    }
  }
  if (mini_gnb_c_b210_probe_apply_rx_profile(usrp,
                                             config->channel,
                                             config->channel_count,
                                             config->rate_sps,
                                             config->rx_gain_db,
                                             config->bandwidth_hz,
                                             config->rx_freq_hz,
                                             error_message,
                                             error_message_size) != 0 ||
      mini_gnb_c_b210_probe_apply_tx_profile(usrp,
                                             config->channel,
                                             config->channel_count,
                                             config->rate_sps,
                                             config->tx_gain_db,
                                             config->bandwidth_hz,
                                             config->tx_freq_hz,
                                             error_message,
                                             error_message_size) != 0) {
    rc = -1;
    goto cleanup;
  }

  (void)uhd_usrp_get_pp_string(usrp, report->device_summary, sizeof(report->device_summary));
  (void)uhd_usrp_get_rx_rate(usrp, config->channel, &report->actual_rate_sps);
  (void)uhd_usrp_get_rx_freq(usrp, config->channel, &report->actual_rx_freq_hz);
  (void)uhd_usrp_get_tx_freq(usrp, config->channel, &report->actual_tx_freq_hz);
  (void)uhd_usrp_get_rx_gain(usrp, config->channel, "", &report->actual_rx_gain_db);
  (void)uhd_usrp_get_tx_gain(usrp, config->channel, "", &report->actual_tx_gain_db);
  (void)uhd_usrp_get_rx_bandwidth(usrp, config->channel, &report->actual_bandwidth_hz);

  if (mini_gnb_c_b210_probe_optional_sensor_bool(usrp,
                                                 true,
                                                 "ref_locked",
                                                 0u,
                                                 &report->ref_locked_valid,
                                                 &report->ref_locked,
                                                 NULL,
                                                 0u) != 0) {
    report->ref_locked_valid = false;
  }
  mini_gnb_c_b210_probe_collect_rx_lo_lock(usrp,
                                           config->channel,
                                           config->channel_count,
                                           &report->rx_lo_locked_valid,
                                           &report->rx_lo_locked);
  mini_gnb_c_b210_probe_collect_tx_lo_lock(usrp,
                                           config->channel,
                                           config->channel_count,
                                           &report->tx_lo_locked_valid,
                                           &report->tx_lo_locked);
  if (config->require_ref_lock && report->ref_locked_valid && !report->ref_locked) {
    rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "reference clock is not locked");
    goto cleanup;
  }
  if (config->require_lo_lock &&
      ((report->rx_lo_locked_valid && !report->rx_lo_locked) || (report->tx_lo_locked_valid && !report->tx_lo_locked))) {
    rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "RX or TX LO is not locked");
    goto cleanup;
  }

  if (mini_gnb_c_b210_probe_fill_channel_list(config->channel,
                                              config->channel_count,
                                              channel_list,
                                              error_message,
                                              error_message_size) != 0) {
    rc = -1;
    goto cleanup;
  }
  if (uhd_rx_streamer_make(&rx_streamer) != UHD_ERROR_NONE ||
      uhd_tx_streamer_make(&tx_streamer) != UHD_ERROR_NONE) {
    rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to create TRX streaming objects");
    goto cleanup;
  }

  memset(&rx_stream_args, 0, sizeof(rx_stream_args));
  rx_stream_args.cpu_format = "sc16";
  rx_stream_args.otw_format = "sc16";
  rx_stream_args.args = "";
  rx_stream_args.channel_list = channel_list;
  rx_stream_args.n_channels = config->channel_count;
  memset(&tx_stream_args, 0, sizeof(tx_stream_args));
  tx_stream_args.cpu_format = "sc16";
  tx_stream_args.otw_format = "sc16";
  tx_stream_args.args = "";
  tx_stream_args.channel_list = channel_list;
  tx_stream_args.n_channels = config->channel_count;

  if (uhd_usrp_get_rx_stream(usrp, &rx_stream_args, rx_streamer) != UHD_ERROR_NONE ||
      uhd_usrp_get_tx_stream(usrp, &tx_stream_args, tx_streamer) != UHD_ERROR_NONE ||
      uhd_rx_streamer_max_num_samps(rx_streamer, &rx_samples_per_buffer) != UHD_ERROR_NONE ||
      uhd_tx_streamer_max_num_samps(tx_streamer, &tx_samples_per_buffer) != UHD_ERROR_NONE ||
      rx_samples_per_buffer == 0u || tx_samples_per_buffer == 0u) {
    rc = mini_gnb_c_b210_probe_fill_uhd_error(usrp, "failed to create TRX streams", error_message, error_message_size);
    goto cleanup;
  }

  if (mini_gnb_c_b210_probe_issue_rx_start(usrp,
                                           rx_streamer,
                                           config->channel_count,
                                           error_message,
                                           error_message_size) != 0) {
    rc = -1;
    goto cleanup;
  }
  wall_start_ns = mini_gnb_c_b210_probe_now_monotonic_ns();
  if (config->duration_mode == MINI_GNB_C_B210_DURATION_MODE_WALLCLOCK && wall_start_ns > 0u) {
    deadline_ns = wall_start_ns + (uint64_t)(config->duration_sec * 1000000000.0);
  }

  rx_worker.rx_streamer = rx_streamer;
  rx_worker.requested_samples = requested_samples;
  rx_worker.samples_per_buffer = rx_samples_per_buffer;
  rx_worker.rate_sps = config->rate_sps;
  rx_worker.duration_mode = config->duration_mode;
  rx_worker.deadline_ns = deadline_ns;
  rx_worker.channel_count = config->channel_count;
  rx_worker.cpu_core = config->rx_cpu_core;
  rx_worker.ring_block_samples = config->ring_block_samples;
  rx_worker.ring = &rx_ring;

  tx_worker.tx_streamer = tx_streamer;
  tx_worker.requested_samples = requested_samples;
  tx_worker.samples_per_buffer = tx_samples_per_buffer;
  tx_worker.channel_count = config->channel_count;
  tx_worker.cpu_core = config->tx_cpu_core;
  tx_worker.tx_ring = &tx_ring;
  tx_worker.tx_prefetch_samples = config->tx_prefetch_samples;
  tx_worker.tx_ring_start_seq = tx_ring.superblock->oldest_valid_seq;
  tx_worker.tx_ring_end_seq = tx_ring.superblock->next_write_seq;
  tx_worker.loop_tx_ring = requested_samples > tx_ring_total_samples;

  if (pthread_create(&rx_thread, NULL, mini_gnb_c_b210_trx_rx_worker_main, &rx_worker) != 0) {
    rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to start TRX RX worker");
    goto cleanup;
  }
  rx_thread_started = true;
  if (pthread_create(&tx_thread, NULL, mini_gnb_c_b210_trx_tx_worker_main, &tx_worker) != 0) {
    rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, "failed to start TRX TX worker");
    goto cleanup;
  }
  tx_thread_started = true;

  (void)pthread_join(rx_thread, NULL);
  rx_thread_started = false;
  (void)pthread_join(tx_thread, NULL);
  tx_thread_started = false;

  if (rx_worker.rc != 0) {
    rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, rx_worker.error_message);
    goto cleanup;
  }
  if (tx_worker.rc != 0) {
    rc = mini_gnb_c_b210_probe_fail(error_message, error_message_size, tx_worker.error_message);
    goto cleanup;
  }

  report->requested_samples = requested_samples;
  report->received_samples = rx_worker.received_samples;
  report->transmitted_samples = tx_worker.transmitted_samples;
  report->duration_mode = config->duration_mode;
  report->used_rx_ring_map = true;
  report->used_tx_ring_map = true;
  report->rx_ring_blocks_committed = rx_worker.ring_blocks_committed;
  report->tx_ring_blocks_committed = tx_worker.ring_blocks_consumed;
  report->tx_prefetch_samples = tx_worker.tx_prefetch_samples;
  report->tx_ring_wrap_count = tx_worker.tx_ring_wrap_count;
  report->rx_overflow_events = rx_worker.rx_overflow_events;
  report->rx_timeout_events = rx_worker.rx_timeout_events;
  report->rx_recoverable_events = rx_worker.rx_recoverable_events;
  report->rx_gap_events = rx_worker.rx_gap_events;
  report->rx_lost_samples_estimate = rx_worker.rx_lost_samples_estimate;
  if (wall_start_ns > 0u) {
    const uint64_t wall_end_ns = mini_gnb_c_b210_probe_now_monotonic_ns();

    if (wall_end_ns >= wall_start_ns) {
      report->wall_elapsed_sec = (double)(wall_end_ns - wall_start_ns) / 1000000000.0;
    }
  }
  report->burst_ack_valid = tx_worker.burst_ack_valid;
  report->burst_ack = tx_worker.burst_ack;
  report->underflow_observed = tx_worker.underflow_observed;
  report->seq_error_observed = tx_worker.seq_error_observed;
  report->time_error_observed = tx_worker.time_error_observed;
  rc = 0;

cleanup:
  if (rx_streamer != NULL) {
    memset(&stream_cmd, 0, sizeof(stream_cmd));
    stream_cmd.stream_mode = UHD_STREAM_MODE_STOP_CONTINUOUS;
    stream_cmd.stream_now = false;
    (void)uhd_rx_streamer_issue_stream_cmd(rx_streamer, &stream_cmd);
  }
  if (rx_thread_started) {
    (void)pthread_join(rx_thread, NULL);
  }
  if (tx_thread_started) {
    (void)pthread_join(tx_thread, NULL);
  }
  mini_gnb_c_sc16_ring_map_close(&rx_ring);
  mini_gnb_c_sc16_ring_map_close(&tx_ring);
  if (tx_streamer != NULL) {
    uhd_tx_streamer_free(&tx_streamer);
  }
  if (rx_streamer != NULL) {
    uhd_rx_streamer_free(&rx_streamer);
  }
  if (tx_subdev_spec != NULL) {
    uhd_subdev_spec_free(&tx_subdev_spec);
  }
  if (rx_subdev_spec != NULL) {
    uhd_subdev_spec_free(&rx_subdev_spec);
  }
  if (usrp != NULL) {
    uhd_usrp_free(&usrp);
  }
  return rc;
}
