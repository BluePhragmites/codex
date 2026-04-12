#define _GNU_SOURCE

#include "mini_gnb_c/radio/b210_slot_backend.h"

#include "mini_gnb_c/common/json_utils.h"
#include "mini_gnb_c/radio/b210_uhd_probe.h"
#include "mini_gnb_c/radio/host_performance.h"
#include "mini_gnb_c/radio/mock_radio_frontend.h"
#include "mini_gnb_c/radio/sc16_ring_map.h"

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __linux__
#include <sched.h>
#endif

#ifdef MINI_GNB_C_HAVE_UHD
#include <uhd.h>
#endif

#define MINI_GNB_C_B210_SLOT_DEFAULT_RX_RING "/dev/shm/mini_gnb_c_radio_frontend_rx_ring.map"
#define MINI_GNB_C_B210_SLOT_DEFAULT_TX_RING "/dev/shm/mini_gnb_c_radio_frontend_tx_ring.map"
#define MINI_GNB_C_B210_SLOT_MAX_CHANNELS 2u

struct mini_gnb_c_b210_slot_backend {
  mini_gnb_c_rf_config_t rf;
  mini_gnb_c_sim_config_t sim;
  mini_gnb_c_mock_radio_frontend_t mock;
  char last_error[256];
  char host_tuning_summary[256];
  bool running;
  bool stop_requested;
  size_t ring_block_samples;
  uint32_t channel_count;
  int current_slot_abs_slot;
  size_t current_slot_sample_count;
  mini_gnb_c_complexf_t current_slot_mix[MINI_GNB_C_MAX_IQ_SAMPLES];
  int16_t* tx_slot_sc16;
  size_t tx_slot_sc16_capacity_samples;
  size_t rx_overflow_events;
  size_t rx_timeout_events;
  size_t rx_recoverable_events;
  size_t rx_gap_events;
  size_t rx_lost_samples_estimate;
  int64_t last_hw_time_ns;
  bool tx_underflow_observed;
  size_t tx_ring_blocks_committed;
  size_t rx_ring_blocks_committed;

#ifdef MINI_GNB_C_HAVE_UHD
  uhd_usrp_handle usrp;
  uhd_rx_streamer_handle rx_streamer;
  uhd_tx_streamer_handle tx_streamer;
  pthread_t rx_thread;
  pthread_t tx_thread;
  bool rx_thread_started;
  bool tx_thread_started;
  size_t rx_samples_per_buffer;
  size_t tx_samples_per_buffer;
  mini_gnb_c_sc16_ring_map_t rx_ring;
  mini_gnb_c_sc16_ring_map_t tx_ring;
#endif
};

static int mini_gnb_c_b210_slot_backend_fail(char* error_message,
                                             size_t error_message_size,
                                             const char* message) {
  if (error_message != NULL && error_message_size > 0u) {
    (void)snprintf(error_message, error_message_size, "%s", message != NULL ? message : "unknown error");
  }
  return -1;
}

static int mini_gnb_c_b210_slot_backend_failf(char* error_message,
                                              size_t error_message_size,
                                              const char* format,
                                              ...) {
  va_list args;

  if (error_message == NULL || error_message_size == 0u) {
    return -1;
  }
  va_start(args, format);
  (void)vsnprintf(error_message, error_message_size, format, args);
  va_end(args);
  return -1;
}

static bool mini_gnb_c_b210_slot_backend_path_is_absolute(const char* path) {
  if (path == NULL || path[0] == '\0') {
    return false;
  }
  if (path[0] == '/' || path[0] == '\\') {
    return true;
  }
  return strlen(path) > 1u && path[1] == ':';
}

static int mini_gnb_c_b210_slot_backend_resolve_path(const char* configured_path,
                                                     const char* fallback_path,
                                                     char* out,
                                                     size_t out_size) {
  if (out == NULL || out_size == 0u) {
    return -1;
  }
  out[0] = '\0';
  if (configured_path != NULL && configured_path[0] != '\0') {
    if (mini_gnb_c_b210_slot_backend_path_is_absolute(configured_path)) {
      return snprintf(out, out_size, "%s", configured_path) < (int)out_size ? 0 : -1;
    }
    return mini_gnb_c_join_path(MINI_GNB_C_SOURCE_DIR, configured_path, out, out_size);
  }
  if (fallback_path == NULL || fallback_path[0] == '\0') {
    return 0;
  }
  return snprintf(out, out_size, "%s", fallback_path) < (int)out_size ? 0 : -1;
}

static int mini_gnb_c_b210_slot_backend_pin_current_thread(const int cpu_core,
                                                           char* error_message,
                                                           const size_t error_message_size) {
#ifdef __linux__
  cpu_set_t set;

  if (cpu_core < 0) {
    return 0;
  }
  if (cpu_core >= CPU_SETSIZE) {
    return mini_gnb_c_b210_slot_backend_failf(error_message,
                                              error_message_size,
                                              "invalid CPU core %d",
                                              cpu_core);
  }
  CPU_ZERO(&set);
  CPU_SET(cpu_core, &set);
  if (sched_setaffinity(0, sizeof(set), &set) != 0) {
    return mini_gnb_c_b210_slot_backend_failf(error_message,
                                              error_message_size,
                                              "failed to pin current thread to CPU %d: %s",
                                              cpu_core,
                                              strerror(errno));
  }
  return 0;
#else
  (void)cpu_core;
  return mini_gnb_c_b210_slot_backend_fail(error_message,
                                           error_message_size,
                                           "CPU affinity for the B210 backend is only supported on Linux");
#endif
}

static uint64_t mini_gnb_c_b210_slot_backend_now_monotonic_ns(void) {
  struct timespec ts;

  memset(&ts, 0, sizeof(ts));
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return 0u;
  }
  return ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;
}

static size_t mini_gnb_c_b210_slot_backend_estimate_lost_samples_from_gap(const uint64_t expected_next_hw_time_ns,
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

static int16_t mini_gnb_c_b210_slot_backend_float_to_sc16(const float value) {
  double scaled = (double)value * 32767.0;

  if (scaled > 32767.0) {
    scaled = 32767.0;
  }
  if (scaled < -32768.0) {
    scaled = -32768.0;
  }
  if (scaled >= 0.0) {
    scaled += 0.5;
  } else {
    scaled -= 0.5;
  }
  return (int16_t)scaled;
}

static void mini_gnb_c_b210_slot_backend_reset_slot_mix(mini_gnb_c_b210_slot_backend_t* backend,
                                                        const int abs_slot) {
  size_t i = 0u;

  if (backend == NULL) {
    return;
  }
  backend->current_slot_abs_slot = abs_slot;
  backend->current_slot_sample_count = 0u;
  for (i = 0u; i < MINI_GNB_C_MAX_IQ_SAMPLES; ++i) {
    backend->current_slot_mix[i].real = 0.0f;
    backend->current_slot_mix[i].imag = 0.0f;
  }
}

static int mini_gnb_c_b210_slot_backend_fill_tx_prefetch_from_ring(mini_gnb_c_sc16_ring_map_t* ring,
                                                                   const uint32_t channel_count,
                                                                   int16_t* sc16_prefetch,
                                                                   const size_t prefetch_capacity_samples,
                                                                   uint64_t* prefetch_seq,
                                                                   size_t* prefetch_block_offset_samples,
                                                                   size_t* prefetch_valid_samples,
                                                                   size_t* prefetch_offset_samples,
                                                                   char* error_message,
                                                                   const size_t error_message_size) {
  uint32_t channel_index = 0u;
  uint64_t ring_start_seq = 0u;
  uint64_t ring_end_seq = 0u;

  if (ring == NULL || ring->superblock == NULL || sc16_prefetch == NULL || prefetch_seq == NULL ||
      prefetch_block_offset_samples == NULL || prefetch_valid_samples == NULL || prefetch_offset_samples == NULL) {
    return mini_gnb_c_b210_slot_backend_fail(error_message, error_message_size, "invalid TX prefetch request");
  }

  ring_start_seq = ring->superblock->oldest_valid_seq;
  ring_end_seq = ring->superblock->next_write_seq;
  if (*prefetch_seq < ring_start_seq) {
    *prefetch_seq = ring_start_seq;
    *prefetch_block_offset_samples = 0u;
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

  while (*prefetch_valid_samples < prefetch_capacity_samples && *prefetch_seq < ring_end_seq) {
    const mini_gnb_c_sc16_ring_map_descriptor_t* descriptor =
        mini_gnb_c_sc16_ring_map_get_descriptor(ring, *prefetch_seq);
    size_t available_in_block = 0u;
    size_t samples_to_copy = 0u;

    if (descriptor == NULL || descriptor->state != MINI_GNB_C_SC16_RING_BLOCK_READY || descriptor->sample_count == 0u) {
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
        return mini_gnb_c_b210_slot_backend_fail(error_message, error_message_size, "failed to resolve TX ring payload");
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
    }
  }

  return 0;
}

#ifdef MINI_GNB_C_HAVE_UHD

static int mini_gnb_c_b210_slot_backend_fill_uhd_error(uhd_usrp_handle usrp,
                                                       const char* prefix,
                                                       char* error_message,
                                                       const size_t error_message_size) {
  char detail[512];

  detail[0] = '\0';
  if (usrp != NULL) {
    (void)uhd_usrp_last_error(usrp, detail, sizeof(detail));
  }
  return mini_gnb_c_b210_slot_backend_failf(error_message,
                                            error_message_size,
                                            "%s%s%s",
                                            prefix != NULL ? prefix : "UHD error",
                                            detail[0] != '\0' ? ": " : "",
                                            detail);
}

static int mini_gnb_c_b210_slot_backend_fill_tx_stream_error(uhd_tx_streamer_handle tx_streamer,
                                                             const char* prefix,
                                                             char* error_message,
                                                             const size_t error_message_size) {
  char detail[512];

  detail[0] = '\0';
  if (tx_streamer != NULL) {
    (void)uhd_tx_streamer_last_error(tx_streamer, detail, sizeof(detail));
  }
  return mini_gnb_c_b210_slot_backend_failf(error_message,
                                            error_message_size,
                                            "%s%s%s",
                                            prefix != NULL ? prefix : "TX stream error",
                                            detail[0] != '\0' ? ": " : "",
                                            detail);
}

static bool mini_gnb_c_b210_slot_backend_string_vector_contains(uhd_string_vector_handle vector, const char* value) {
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

static int mini_gnb_c_b210_slot_backend_optional_sensor_bool(uhd_usrp_handle usrp,
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
    return mini_gnb_c_b210_slot_backend_fail(error_message, error_message_size, "failed to allocate sensor name list");
  }
  if ((mboard_sensor ? uhd_usrp_get_mboard_sensor_names(usrp, index, &names)
                     : uhd_usrp_get_rx_sensor_names(usrp, index, &names)) != UHD_ERROR_NONE) {
    goto cleanup;
  }
  if (!mini_gnb_c_b210_slot_backend_string_vector_contains(names, sensor_name)) {
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
    return mini_gnb_c_b210_slot_backend_failf(error_message,
                                              error_message_size,
                                              "failed to query sensor %s",
                                              sensor_name);
  }
  return 0;
}

static int mini_gnb_c_b210_slot_backend_optional_tx_sensor_bool(uhd_usrp_handle usrp,
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
    return mini_gnb_c_b210_slot_backend_fail(error_message, error_message_size, "failed to allocate TX sensor names");
  }
  if (uhd_usrp_get_tx_sensor_names(usrp, channel, &names) != UHD_ERROR_NONE) {
    goto cleanup;
  }
  if (!mini_gnb_c_b210_slot_backend_string_vector_contains(names, sensor_name)) {
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
    return mini_gnb_c_b210_slot_backend_failf(error_message,
                                              error_message_size,
                                              "failed to query TX sensor %s",
                                              sensor_name);
  }
  return 0;
}

static int mini_gnb_c_b210_slot_backend_fill_channel_list(const size_t first_channel,
                                                          const uint32_t channel_count,
                                                          size_t* channel_list,
                                                          char* error_message,
                                                          const size_t error_message_size) {
  uint32_t i = 0u;

  if (channel_count == 0u || channel_count > MINI_GNB_C_B210_SLOT_MAX_CHANNELS || channel_list == NULL) {
    return mini_gnb_c_b210_slot_backend_fail(error_message, error_message_size, "invalid channel count");
  }
  if (first_channel + channel_count > MINI_GNB_C_B210_SLOT_MAX_CHANNELS) {
    return mini_gnb_c_b210_slot_backend_fail(error_message, error_message_size, "channel range exceeds B210 channel count");
  }
  for (i = 0u; i < channel_count; ++i) {
    channel_list[i] = first_channel + i;
  }
  return 0;
}

static int mini_gnb_c_b210_slot_backend_apply_rx_profile(uhd_usrp_handle usrp,
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
      return mini_gnb_c_b210_slot_backend_fill_uhd_error(usrp,
                                                         "failed to apply RX settings",
                                                         error_message,
                                                         error_message_size);
    }
  }
  return 0;
}

static int mini_gnb_c_b210_slot_backend_apply_tx_profile(uhd_usrp_handle usrp,
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
      return mini_gnb_c_b210_slot_backend_fill_uhd_error(usrp,
                                                         "failed to apply TX settings",
                                                         error_message,
                                                         error_message_size);
    }
  }
  return 0;
}

static void mini_gnb_c_b210_slot_backend_collect_rx_lo_lock(uhd_usrp_handle usrp,
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

    if (mini_gnb_c_b210_slot_backend_optional_sensor_bool(usrp,
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

static void mini_gnb_c_b210_slot_backend_collect_tx_lo_lock(uhd_usrp_handle usrp,
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

    if (mini_gnb_c_b210_slot_backend_optional_tx_sensor_bool(usrp,
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

static int mini_gnb_c_b210_slot_backend_issue_rx_start(uhd_usrp_handle usrp,
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
      return mini_gnb_c_b210_slot_backend_fill_uhd_error(usrp,
                                                         "failed to get current USRP time",
                                                         error_message,
                                                         error_message_size);
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
    return mini_gnb_c_b210_slot_backend_fail(error_message, error_message_size, "failed to issue RX stream command");
  }
  return 0;
}

static int mini_gnb_c_b210_slot_backend_rx_metadata_time_ns(uhd_rx_metadata_handle md,
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
    return mini_gnb_c_b210_slot_backend_fail(error_message, error_message_size, "invalid RX metadata time request");
  }
  if (uhd_rx_metadata_has_time_spec(md, &has_time_spec) != UHD_ERROR_NONE) {
    return mini_gnb_c_b210_slot_backend_fail(error_message, error_message_size, "failed to query RX metadata time");
  }
  if (!has_time_spec) {
    return 0;
  }
  if (uhd_rx_metadata_time_spec(md, &full_secs, &frac_secs) != UHD_ERROR_NONE) {
    return mini_gnb_c_b210_slot_backend_fail(error_message, error_message_size, "failed to read RX metadata time");
  }
  *time_ns_out = mini_gnb_c_b210_time_spec_to_ns(full_secs, frac_secs);
  *valid_out = true;
  return 0;
}

static void* mini_gnb_c_b210_slot_backend_rx_worker_main(void* arg) {
  mini_gnb_c_b210_slot_backend_t* backend = (mini_gnb_c_b210_slot_backend_t*)arg;
  uhd_rx_metadata_handle md = NULL;
  int16_t* channel_buffers[MINI_GNB_C_B210_SLOT_MAX_CHANNELS] = {NULL, NULL};
  int16_t* ring_staging = NULL;
  size_t ring_staging_samples = 0u;
  uint64_t ring_staging_hw_time_ns = 0u;
  uint32_t ring_staging_flags = 0u;
  uint64_t prev_expected_next_hw_time_ns = 0u;
  uint32_t channel_index = 0u;

  if (backend == NULL) {
    return NULL;
  }
  if (mini_gnb_c_b210_slot_backend_pin_current_thread(backend->rf.rx_cpu_core,
                                                      backend->last_error,
                                                      sizeof(backend->last_error)) != 0) {
    return NULL;
  }
  if (uhd_rx_metadata_make(&md) != UHD_ERROR_NONE) {
    (void)snprintf(backend->last_error, sizeof(backend->last_error), "%s", "failed to allocate RX metadata");
    return NULL;
  }
  for (channel_index = 0u; channel_index < backend->channel_count; ++channel_index) {
    channel_buffers[channel_index] =
        (int16_t*)malloc(backend->rx_samples_per_buffer * 2u * sizeof(int16_t));
    if (channel_buffers[channel_index] == NULL) {
      (void)snprintf(backend->last_error, sizeof(backend->last_error), "%s", "failed to allocate RX channel buffer");
      goto cleanup;
    }
  }
  ring_staging =
      (int16_t*)malloc(backend->ring_block_samples * backend->channel_count * 2u * sizeof(int16_t));
  if (ring_staging == NULL) {
    (void)snprintf(backend->last_error, sizeof(backend->last_error), "%s", "failed to allocate RX ring staging");
    goto cleanup;
  }

  while (!backend->stop_requested) {
    void* buffer_ptrs[MINI_GNB_C_B210_SLOT_MAX_CHANNELS] = {NULL, NULL};
    size_t samples_received = 0u;
    uhd_rx_metadata_error_code_t error_code = UHD_RX_METADATA_ERROR_CODE_NONE;
    uint64_t recv_hw_time_ns = 0u;
    bool recv_hw_time_valid = false;
    size_t offset_samples = 0u;

    for (channel_index = 0u; channel_index < backend->channel_count; ++channel_index) {
      buffer_ptrs[channel_index] = channel_buffers[channel_index];
    }
    if (uhd_rx_streamer_recv(backend->rx_streamer,
                             buffer_ptrs,
                             backend->rx_samples_per_buffer,
                             &md,
                             0.1,
                             false,
                             &samples_received) != UHD_ERROR_NONE) {
      (void)snprintf(backend->last_error, sizeof(backend->last_error), "%s", "failed while receiving slot backend samples");
      goto cleanup;
    }
    if (uhd_rx_metadata_error_code(md, &error_code) != UHD_ERROR_NONE) {
      (void)snprintf(backend->last_error, sizeof(backend->last_error), "%s", "failed to decode RX metadata");
      goto cleanup;
    }
    if (error_code == UHD_RX_METADATA_ERROR_CODE_OVERFLOW ||
        error_code == UHD_RX_METADATA_ERROR_CODE_TIMEOUT) {
      if (error_code == UHD_RX_METADATA_ERROR_CODE_OVERFLOW) {
        backend->rx_overflow_events += 1u;
      } else {
        backend->rx_timeout_events += 1u;
      }
      backend->rx_recoverable_events += 1u;
      continue;
    }
    if (error_code != UHD_RX_METADATA_ERROR_CODE_NONE) {
      (void)snprintf(backend->last_error, sizeof(backend->last_error), "fatal RX metadata error code %d", (int)error_code);
      goto cleanup;
    }
    if (samples_received == 0u) {
      continue;
    }
    if (mini_gnb_c_b210_slot_backend_rx_metadata_time_ns(md,
                                                         &recv_hw_time_ns,
                                                         &recv_hw_time_valid,
                                                         backend->last_error,
                                                         sizeof(backend->last_error)) != 0) {
      goto cleanup;
    }
    if (recv_hw_time_valid) {
      backend->last_hw_time_ns = (int64_t)recv_hw_time_ns;
      if (prev_expected_next_hw_time_ns > 0u && recv_hw_time_ns > prev_expected_next_hw_time_ns) {
        size_t lost_samples = mini_gnb_c_b210_slot_backend_estimate_lost_samples_from_gap(prev_expected_next_hw_time_ns,
                                                                                           recv_hw_time_ns,
                                                                                           backend->rf.srate);
        if (lost_samples > 0u) {
          backend->rx_gap_events += 1u;
          backend->rx_lost_samples_estimate += lost_samples;
        }
      }
    }

    while (offset_samples < samples_received) {
      const size_t free_samples = backend->ring_block_samples - ring_staging_samples;
      const size_t copy_samples =
          (samples_received - offset_samples) > free_samples ? free_samples : (samples_received - offset_samples);

      if (ring_staging_samples == 0u) {
        if (recv_hw_time_valid) {
          ring_staging_hw_time_ns = mini_gnb_c_b210_advance_time_ns(recv_hw_time_ns, offset_samples, backend->rf.srate);
          ring_staging_flags = MINI_GNB_C_SC16_RING_FLAG_HW_TIME_VALID;
        } else {
          ring_staging_hw_time_ns = mini_gnb_c_b210_slot_backend_now_monotonic_ns();
          ring_staging_flags = MINI_GNB_C_SC16_RING_FLAG_HOST_TIME_FALLBACK;
        }
      }

      for (channel_index = 0u; channel_index < backend->channel_count; ++channel_index) {
        memcpy(ring_staging + (((size_t)channel_index * backend->ring_block_samples + ring_staging_samples) * 2u),
               channel_buffers[channel_index] + (offset_samples * 2u),
               copy_samples * 2u * sizeof(int16_t));
      }
      ring_staging_samples += copy_samples;
      offset_samples += copy_samples;

      if (ring_staging_samples == backend->ring_block_samples) {
        if (mini_gnb_c_sc16_ring_map_append(&backend->rx_ring,
                                            ring_staging,
                                            (uint32_t)ring_staging_samples,
                                            ring_staging_hw_time_ns,
                                            ring_staging_flags,
                                            backend->last_error,
                                            sizeof(backend->last_error)) != 0) {
          goto cleanup;
        }
        backend->rx_ring_blocks_committed += 1u;
        ring_staging_samples = 0u;
        ring_staging_hw_time_ns = 0u;
        ring_staging_flags = 0u;
      }
    }
    if (recv_hw_time_valid) {
      prev_expected_next_hw_time_ns =
          mini_gnb_c_b210_advance_time_ns(recv_hw_time_ns, samples_received, backend->rf.srate);
    }
  }

  if (ring_staging_samples > 0u) {
    if (mini_gnb_c_sc16_ring_map_append(&backend->rx_ring,
                                        ring_staging,
                                        (uint32_t)ring_staging_samples,
                                        ring_staging_hw_time_ns,
                                        ring_staging_flags,
                                        backend->last_error,
                                        sizeof(backend->last_error)) != 0) {
      goto cleanup;
    }
    backend->rx_ring_blocks_committed += 1u;
  }

cleanup:
  if (ring_staging != NULL) {
    free(ring_staging);
  }
  for (channel_index = 0u; channel_index < MINI_GNB_C_B210_SLOT_MAX_CHANNELS; ++channel_index) {
    if (channel_buffers[channel_index] != NULL) {
      free(channel_buffers[channel_index]);
    }
  }
  if (md != NULL) {
    uhd_rx_metadata_free(&md);
  }
  return NULL;
}

static void* mini_gnb_c_b210_slot_backend_tx_worker_main(void* arg) {
  mini_gnb_c_b210_slot_backend_t* backend = (mini_gnb_c_b210_slot_backend_t*)arg;
  uhd_async_metadata_handle async_md = NULL;
  uhd_tx_metadata_handle start_md = NULL;
  uhd_tx_metadata_handle middle_md = NULL;
  int16_t* sc16_prefetch = NULL;
  int16_t* zero_buffer = NULL;
  size_t prefetch_capacity_samples = 0u;
  size_t prefetch_valid_samples = 0u;
  size_t prefetch_offset_samples = 0u;
  size_t prefetch_block_offset_samples = 0u;
  uint64_t prefetch_seq = 0u;
  bool first_send = true;

  if (backend == NULL) {
    return NULL;
  }
  if (mini_gnb_c_b210_slot_backend_pin_current_thread(backend->rf.tx_cpu_core,
                                                      backend->last_error,
                                                      sizeof(backend->last_error)) != 0) {
    return NULL;
  }
  if (uhd_async_metadata_make(&async_md) != UHD_ERROR_NONE ||
      uhd_tx_metadata_make(&start_md, false, 0, 0.0, true, false) != UHD_ERROR_NONE ||
      uhd_tx_metadata_make(&middle_md, false, 0, 0.0, false, false) != UHD_ERROR_NONE) {
    (void)snprintf(backend->last_error, sizeof(backend->last_error), "%s", "failed to allocate TX metadata");
    goto cleanup;
  }

  prefetch_capacity_samples = backend->rf.tx_prefetch_samples;
  if (prefetch_capacity_samples == 0u) {
    prefetch_capacity_samples = backend->tx_samples_per_buffer * 64u;
  }
  if (prefetch_capacity_samples < backend->ring_block_samples) {
    prefetch_capacity_samples = backend->ring_block_samples;
  }
  if (prefetch_capacity_samples < backend->tx_samples_per_buffer) {
    prefetch_capacity_samples = backend->tx_samples_per_buffer;
  }
  sc16_prefetch = (int16_t*)malloc(prefetch_capacity_samples * backend->channel_count * 2u * sizeof(int16_t));
  zero_buffer = (int16_t*)calloc(backend->tx_samples_per_buffer * backend->channel_count * 2u, sizeof(int16_t));
  if (sc16_prefetch == NULL || zero_buffer == NULL) {
    (void)snprintf(backend->last_error, sizeof(backend->last_error), "%s", "failed to allocate TX staging buffers");
    goto cleanup;
  }

  prefetch_seq = backend->tx_ring.superblock->oldest_valid_seq;

  while (!backend->stop_requested) {
    const size_t prefetch_low_watermark = prefetch_capacity_samples / 2u;
    size_t samples_to_send = backend->tx_samples_per_buffer;
    size_t samples_sent = 0u;
    const void* buffer_ptrs[MINI_GNB_C_B210_SLOT_MAX_CHANNELS] = {NULL, NULL};
    uhd_tx_metadata_handle md = first_send ? start_md : middle_md;
    uint32_t channel_index = 0u;

    if (prefetch_valid_samples <= prefetch_low_watermark &&
        mini_gnb_c_b210_slot_backend_fill_tx_prefetch_from_ring(&backend->tx_ring,
                                                                backend->channel_count,
                                                                sc16_prefetch,
                                                                prefetch_capacity_samples,
                                                                &prefetch_seq,
                                                                &prefetch_block_offset_samples,
                                                                &prefetch_valid_samples,
                                                                &prefetch_offset_samples,
                                                                backend->last_error,
                                                                sizeof(backend->last_error)) != 0) {
      goto cleanup;
    }

    if (prefetch_valid_samples > 0u) {
      if (samples_to_send > prefetch_valid_samples) {
        samples_to_send = prefetch_valid_samples;
      }
      for (channel_index = 0u; channel_index < backend->channel_count; ++channel_index) {
        buffer_ptrs[channel_index] =
            sc16_prefetch + (((size_t)channel_index * prefetch_capacity_samples + prefetch_offset_samples) * 2u);
      }
    } else {
      for (channel_index = 0u; channel_index < backend->channel_count; ++channel_index) {
        buffer_ptrs[channel_index] =
            zero_buffer + ((size_t)channel_index * backend->tx_samples_per_buffer * 2u);
      }
    }

    if (uhd_tx_streamer_send(backend->tx_streamer, buffer_ptrs, samples_to_send, &md, 1.0, &samples_sent) !=
        UHD_ERROR_NONE) {
      (void)mini_gnb_c_b210_slot_backend_fill_tx_stream_error(backend->tx_streamer,
                                                              "failed while transmitting slot backend samples",
                                                              backend->last_error,
                                                              sizeof(backend->last_error));
      goto cleanup;
    }
    if (samples_sent != samples_to_send) {
      (void)snprintf(backend->last_error, sizeof(backend->last_error), "%s", "partial TX send in slot backend");
      goto cleanup;
    }
    first_send = false;
    if (prefetch_valid_samples > 0u) {
      prefetch_offset_samples += samples_sent;
      prefetch_valid_samples -= samples_sent;
    }

    for (;;) {
      bool valid = false;
      uhd_async_metadata_event_code_t event_code = 0;

      if (uhd_tx_streamer_recv_async_msg(backend->tx_streamer, &async_md, 0.0, &valid) != UHD_ERROR_NONE) {
        break;
      }
      if (!valid) {
        break;
      }
      if (uhd_async_metadata_event_code(async_md, &event_code) != UHD_ERROR_NONE) {
        break;
      }
      if (event_code == UHD_ASYNC_METADATA_EVENT_CODE_UNDERFLOW ||
          event_code == UHD_ASYNC_METADATA_EVENT_CODE_UNDERFLOW_IN_PACKET) {
        backend->tx_underflow_observed = true;
      }
    }
  }

cleanup:
  if (zero_buffer != NULL) {
    free(zero_buffer);
  }
  if (sc16_prefetch != NULL) {
    free(sc16_prefetch);
  }
  if (middle_md != NULL) {
    uhd_tx_metadata_free(&middle_md);
  }
  if (start_md != NULL) {
    uhd_tx_metadata_free(&start_md);
  }
  if (async_md != NULL) {
    uhd_async_metadata_free(&async_md);
  }
  return NULL;
}

static int mini_gnb_c_b210_slot_backend_start_threads(mini_gnb_c_b210_slot_backend_t* backend,
                                                      char* error_message,
                                                      const size_t error_message_size) {
  if (backend == NULL) {
    return -1;
  }
  if (pthread_create(&backend->rx_thread, NULL, mini_gnb_c_b210_slot_backend_rx_worker_main, backend) != 0) {
    return mini_gnb_c_b210_slot_backend_fail(error_message, error_message_size, "failed to start B210 RX worker");
  }
  backend->rx_thread_started = true;
  if (pthread_create(&backend->tx_thread, NULL, mini_gnb_c_b210_slot_backend_tx_worker_main, backend) != 0) {
    return mini_gnb_c_b210_slot_backend_fail(error_message, error_message_size, "failed to start B210 TX worker");
  }
  backend->tx_thread_started = true;
  backend->running = true;
  return 0;
}

#endif

int mini_gnb_c_b210_slot_backend_create(mini_gnb_c_b210_slot_backend_t** out_backend,
                                        const mini_gnb_c_rf_config_t* rf_config,
                                        const mini_gnb_c_sim_config_t* sim_config,
                                        char* error_message,
                                        const size_t error_message_size) {
  mini_gnb_c_b210_slot_backend_t* backend = NULL;
  char rx_ring_path[MINI_GNB_C_MAX_PATH];
  char tx_ring_path[MINI_GNB_C_MAX_PATH];

  if (out_backend == NULL || rf_config == NULL || sim_config == NULL) {
    return mini_gnb_c_b210_slot_backend_fail(error_message, error_message_size, "invalid B210 slot backend arguments");
  }
  *out_backend = NULL;
  if (rf_config->channel_count == 0u || rf_config->channel_count > MINI_GNB_C_B210_SLOT_MAX_CHANNELS) {
    return mini_gnb_c_b210_slot_backend_fail(error_message, error_message_size, "B210 slot backend requires channel_count 1 or 2");
  }
  if (rf_config->ring_block_samples < MINI_GNB_C_MAX_IQ_SAMPLES) {
    return mini_gnb_c_b210_slot_backend_failf(error_message,
                                              error_message_size,
                                              "B210 slot backend requires rf.ring_block_samples >= %u",
                                              (unsigned)MINI_GNB_C_MAX_IQ_SAMPLES);
  }
  if (mini_gnb_c_b210_slot_backend_resolve_path(rf_config->rx_ring_map,
                                                MINI_GNB_C_B210_SLOT_DEFAULT_RX_RING,
                                                rx_ring_path,
                                                sizeof(rx_ring_path)) != 0 ||
      mini_gnb_c_b210_slot_backend_resolve_path(rf_config->tx_ring_map,
                                                MINI_GNB_C_B210_SLOT_DEFAULT_TX_RING,
                                                tx_ring_path,
                                                sizeof(tx_ring_path)) != 0) {
    return mini_gnb_c_b210_slot_backend_fail(error_message, error_message_size, "failed to resolve B210 ring-map paths");
  }
  if (strcmp(rx_ring_path, tx_ring_path) == 0) {
    return mini_gnb_c_b210_slot_backend_fail(error_message,
                                             error_message_size,
                                             "B210 slot backend requires different RX and TX ring-map paths");
  }

  backend = (mini_gnb_c_b210_slot_backend_t*)calloc(1u, sizeof(*backend));
  if (backend == NULL) {
    return mini_gnb_c_b210_slot_backend_fail(error_message, error_message_size, "failed to allocate B210 slot backend");
  }
  backend->rf = *rf_config;
  backend->sim = *sim_config;
  backend->channel_count = rf_config->channel_count;
  backend->ring_block_samples = rf_config->ring_block_samples;
  backend->current_slot_abs_slot = -1;
  backend->tx_slot_sc16_capacity_samples = backend->ring_block_samples;
  backend->tx_slot_sc16 =
      (int16_t*)calloc(backend->tx_slot_sc16_capacity_samples * backend->channel_count * 2u, sizeof(int16_t));
  if (backend->tx_slot_sc16 == NULL) {
    free(backend);
    return mini_gnb_c_b210_slot_backend_fail(error_message, error_message_size, "failed to allocate TX slot staging");
  }
  mini_gnb_c_mock_radio_frontend_init(&backend->mock, rf_config, sim_config);
  mini_gnb_c_b210_slot_backend_reset_slot_mix(backend, -1);

#ifndef MINI_GNB_C_HAVE_UHD
  (void)snprintf(backend->last_error,
                 sizeof(backend->last_error),
                 "%s",
                 "B210 slot backend requires a UHD-enabled build.");
  if (error_message != NULL && error_message_size > 0u) {
    (void)snprintf(error_message, error_message_size, "%s", backend->last_error);
  }
  mini_gnb_c_b210_slot_backend_destroy(&backend);
  return -1;
#else
  {
    mini_gnb_c_sc16_ring_map_config_t ring_config;
    size_t channel_list[MINI_GNB_C_B210_SLOT_MAX_CHANNELS] = {0u, 0u};
    uhd_stream_args_t rx_stream_args;
    uhd_stream_args_t tx_stream_args;
    bool ref_locked_valid = false;
    bool ref_locked = false;
    bool rx_lo_locked_valid = false;
    bool rx_lo_locked = false;
    bool tx_lo_locked_valid = false;
    bool tx_lo_locked = false;
    uhd_subdev_spec_handle rx_subdev_spec = NULL;
    uhd_subdev_spec_handle tx_subdev_spec = NULL;

    backend->rx_ring.fd = -1;
    backend->tx_ring.fd = -1;
    mini_gnb_c_sc16_ring_map_config_init(&ring_config);

    if (rf_config->apply_host_tuning &&
        mini_gnb_c_radio_host_performance_prepare_for_backend(MINI_GNB_C_RADIO_BACKEND_B210,
                                                              backend->host_tuning_summary,
                                                              sizeof(backend->host_tuning_summary)) != 0) {
      mini_gnb_c_b210_slot_backend_destroy(&backend);
      return mini_gnb_c_b210_slot_backend_fail(error_message, error_message_size, "failed to prepare host tuning");
    }

    ring_config.role = MINI_GNB_C_SC16_RING_ROLE_RX;
    ring_config.channel_count = backend->channel_count;
    ring_config.block_count = rf_config->ring_block_count;
    ring_config.block_samples = rf_config->ring_block_samples;
    ring_config.sample_rate_sps = (uint64_t)llround(rf_config->srate);
    if (mini_gnb_c_sc16_ring_map_create(rx_ring_path,
                                        &ring_config,
                                        &backend->rx_ring,
                                        backend->last_error,
                                        sizeof(backend->last_error)) != 0) {
      goto fail;
    }
    ring_config.role = MINI_GNB_C_SC16_RING_ROLE_TX;
    if (mini_gnb_c_sc16_ring_map_create(tx_ring_path,
                                        &ring_config,
                                        &backend->tx_ring,
                                        backend->last_error,
                                        sizeof(backend->last_error)) != 0) {
      goto fail;
    }

    if (uhd_usrp_make(&backend->usrp, rf_config->device_args) != UHD_ERROR_NONE) {
      (void)snprintf(backend->last_error, sizeof(backend->last_error), "%s", "failed to create B210 device");
      goto fail;
    }
    if (rf_config->clock_src[0] != '\0' &&
        uhd_usrp_set_clock_source(backend->usrp, rf_config->clock_src, 0u) != UHD_ERROR_NONE) {
      (void)mini_gnb_c_b210_slot_backend_fill_uhd_error(backend->usrp,
                                                        "failed to set clock source",
                                                        backend->last_error,
                                                        sizeof(backend->last_error));
      goto fail;
    }
    if (rf_config->subdev[0] != '\0') {
      if (uhd_subdev_spec_make(&rx_subdev_spec, rf_config->subdev) != UHD_ERROR_NONE ||
          uhd_subdev_spec_make(&tx_subdev_spec, rf_config->subdev) != UHD_ERROR_NONE) {
        (void)snprintf(backend->last_error, sizeof(backend->last_error), "%s", "failed to create TRX subdevice spec");
        goto fail;
      }
      if (uhd_usrp_set_rx_subdev_spec(backend->usrp, rx_subdev_spec, 0u) != UHD_ERROR_NONE ||
          uhd_usrp_set_tx_subdev_spec(backend->usrp, tx_subdev_spec, 0u) != UHD_ERROR_NONE) {
        (void)mini_gnb_c_b210_slot_backend_fill_uhd_error(backend->usrp,
                                                          "failed to set TRX subdevice",
                                                          backend->last_error,
                                                          sizeof(backend->last_error));
        goto fail;
      }
    }

    if (mini_gnb_c_b210_slot_backend_apply_rx_profile(backend->usrp,
                                                      rf_config->channel,
                                                      rf_config->channel_count,
                                                      rf_config->srate,
                                                      rf_config->rx_gain,
                                                      rf_config->bandwidth_hz,
                                                      rf_config->rx_freq_hz,
                                                      backend->last_error,
                                                      sizeof(backend->last_error)) != 0 ||
        mini_gnb_c_b210_slot_backend_apply_tx_profile(backend->usrp,
                                                      rf_config->channel,
                                                      rf_config->channel_count,
                                                      rf_config->srate,
                                                      rf_config->tx_gain,
                                                      rf_config->bandwidth_hz,
                                                      rf_config->tx_freq_hz,
                                                      backend->last_error,
                                                      sizeof(backend->last_error)) != 0) {
      goto fail;
    }

    if (mini_gnb_c_b210_slot_backend_optional_sensor_bool(backend->usrp,
                                                          true,
                                                          "ref_locked",
                                                          0u,
                                                          &ref_locked_valid,
                                                          &ref_locked,
                                                          NULL,
                                                          0u) != 0) {
      ref_locked_valid = false;
    }
    mini_gnb_c_b210_slot_backend_collect_rx_lo_lock(backend->usrp,
                                                    rf_config->channel,
                                                    rf_config->channel_count,
                                                    &rx_lo_locked_valid,
                                                    &rx_lo_locked);
    mini_gnb_c_b210_slot_backend_collect_tx_lo_lock(backend->usrp,
                                                    rf_config->channel,
                                                    rf_config->channel_count,
                                                    &tx_lo_locked_valid,
                                                    &tx_lo_locked);
    if (rf_config->require_ref_lock && ref_locked_valid && !ref_locked) {
      (void)snprintf(backend->last_error, sizeof(backend->last_error), "%s", "reference clock is not locked");
      goto fail;
    }
    if (rf_config->require_lo_lock &&
        ((rx_lo_locked_valid && !rx_lo_locked) || (tx_lo_locked_valid && !tx_lo_locked))) {
      (void)snprintf(backend->last_error, sizeof(backend->last_error), "%s", "RX or TX LO is not locked");
      goto fail;
    }

    if (mini_gnb_c_b210_slot_backend_fill_channel_list(rf_config->channel,
                                                       rf_config->channel_count,
                                                       channel_list,
                                                       backend->last_error,
                                                       sizeof(backend->last_error)) != 0) {
      goto fail;
    }
    if (uhd_rx_streamer_make(&backend->rx_streamer) != UHD_ERROR_NONE ||
        uhd_tx_streamer_make(&backend->tx_streamer) != UHD_ERROR_NONE) {
      (void)snprintf(backend->last_error, sizeof(backend->last_error), "%s", "failed to create TRX streamers");
      goto fail;
    }

    memset(&rx_stream_args, 0, sizeof(rx_stream_args));
    rx_stream_args.cpu_format = "sc16";
    rx_stream_args.otw_format = "sc16";
    rx_stream_args.args = "";
    rx_stream_args.channel_list = channel_list;
    rx_stream_args.n_channels = rf_config->channel_count;
    memset(&tx_stream_args, 0, sizeof(tx_stream_args));
    tx_stream_args.cpu_format = "sc16";
    tx_stream_args.otw_format = "sc16";
    tx_stream_args.args = "";
    tx_stream_args.channel_list = channel_list;
    tx_stream_args.n_channels = rf_config->channel_count;

    if (uhd_usrp_get_rx_stream(backend->usrp, &rx_stream_args, backend->rx_streamer) != UHD_ERROR_NONE ||
        uhd_usrp_get_tx_stream(backend->usrp, &tx_stream_args, backend->tx_streamer) != UHD_ERROR_NONE ||
        uhd_rx_streamer_max_num_samps(backend->rx_streamer, &backend->rx_samples_per_buffer) != UHD_ERROR_NONE ||
        uhd_tx_streamer_max_num_samps(backend->tx_streamer, &backend->tx_samples_per_buffer) != UHD_ERROR_NONE ||
        backend->rx_samples_per_buffer == 0u || backend->tx_samples_per_buffer == 0u) {
      (void)mini_gnb_c_b210_slot_backend_fill_uhd_error(backend->usrp,
                                                        "failed to create TRX streams",
                                                        backend->last_error,
                                                        sizeof(backend->last_error));
      goto fail;
    }
    if (mini_gnb_c_b210_slot_backend_issue_rx_start(backend->usrp,
                                                    backend->rx_streamer,
                                                    rf_config->channel_count,
                                                    backend->last_error,
                                                    sizeof(backend->last_error)) != 0) {
      goto fail;
    }
    if (mini_gnb_c_b210_slot_backend_start_threads(backend,
                                                   backend->last_error,
                                                   sizeof(backend->last_error)) != 0) {
      goto fail;
    }

    if (tx_subdev_spec != NULL) {
      uhd_subdev_spec_free(&tx_subdev_spec);
    }
    if (rx_subdev_spec != NULL) {
      uhd_subdev_spec_free(&rx_subdev_spec);
    }
  }

  *out_backend = backend;
  return 0;

fail:
  if (error_message != NULL && error_message_size > 0u) {
    (void)snprintf(error_message, error_message_size, "%s", backend->last_error);
  }
  mini_gnb_c_b210_slot_backend_destroy(&backend);
  return -1;
#endif
}

const char* mini_gnb_c_b210_slot_backend_error(const mini_gnb_c_b210_slot_backend_t* backend) {
  if (backend == NULL || backend->last_error[0] == '\0') {
    return "";
  }
  return backend->last_error;
}

bool mini_gnb_c_b210_slot_backend_has_pucch_sr_armed_for(const mini_gnb_c_b210_slot_backend_t* backend,
                                                         const uint16_t rnti,
                                                         const int abs_slot,
                                                         const int current_abs_slot) {
  if (backend == NULL) {
    return false;
  }
  return backend->mock.pucch_sr_armed && backend->mock.pucch_sr_abs_slot >= current_abs_slot &&
         backend->mock.pucch_sr_abs_slot == abs_slot && backend->mock.pucch_sr_rnti == rnti;
}

uint64_t mini_gnb_c_b210_slot_backend_tx_burst_count(const mini_gnb_c_b210_slot_backend_t* backend) {
  return backend != NULL ? backend->mock.tx_burst_count : 0u;
}

int64_t mini_gnb_c_b210_slot_backend_last_hw_time_ns(const mini_gnb_c_b210_slot_backend_t* backend) {
  return backend != NULL ? backend->last_hw_time_ns : 0;
}

void mini_gnb_c_b210_slot_backend_receive(mini_gnb_c_b210_slot_backend_t* backend,
                                          const mini_gnb_c_slot_indication_t* slot,
                                          mini_gnb_c_radio_burst_t* out_burst) {
  if (backend == NULL) {
    return;
  }
  mini_gnb_c_mock_radio_frontend_receive(&backend->mock, slot, out_burst);
}

void mini_gnb_c_b210_slot_backend_arm_msg3(mini_gnb_c_b210_slot_backend_t* backend,
                                           const mini_gnb_c_ul_grant_for_msg3_t* ul_grant) {
  if (backend == NULL) {
    return;
  }
  mini_gnb_c_mock_radio_frontend_arm_msg3(&backend->mock, ul_grant);
}

void mini_gnb_c_b210_slot_backend_arm_pucch_sr(mini_gnb_c_b210_slot_backend_t* backend,
                                               const uint16_t rnti,
                                               const int abs_slot) {
  if (backend == NULL) {
    return;
  }
  mini_gnb_c_mock_radio_frontend_arm_pucch_sr(&backend->mock, rnti, abs_slot);
}

void mini_gnb_c_b210_slot_backend_arm_dl_ack(mini_gnb_c_b210_slot_backend_t* backend,
                                             const uint16_t rnti,
                                             const uint8_t harq_id,
                                             const int abs_slot) {
  if (backend == NULL) {
    return;
  }
  mini_gnb_c_mock_radio_frontend_arm_dl_ack(&backend->mock, rnti, harq_id, abs_slot);
}

void mini_gnb_c_b210_slot_backend_arm_ul_data(mini_gnb_c_b210_slot_backend_t* backend,
                                              const mini_gnb_c_ul_data_grant_t* ul_grant) {
  if (backend == NULL) {
    return;
  }
  mini_gnb_c_mock_radio_frontend_arm_ul_data(&backend->mock, ul_grant);
}

void mini_gnb_c_b210_slot_backend_stage_ue_ipv4(mini_gnb_c_b210_slot_backend_t* backend,
                                                const uint8_t ue_ipv4[4],
                                                const bool ue_ipv4_valid) {
  if (backend == NULL) {
    return;
  }
  mini_gnb_c_mock_radio_frontend_stage_ue_ipv4(&backend->mock, ue_ipv4, ue_ipv4_valid);
}

void mini_gnb_c_b210_slot_backend_submit_tx(mini_gnb_c_b210_slot_backend_t* backend,
                                            const mini_gnb_c_slot_indication_t* slot,
                                            const mini_gnb_c_tx_grid_patch_t* patches,
                                            const size_t patch_count,
                                            struct mini_gnb_c_metrics_trace* metrics) {
  size_t patch_index = 0u;

  if (backend == NULL || slot == NULL || patches == NULL || metrics == NULL) {
    return;
  }

  mini_gnb_c_mock_radio_frontend_submit_tx(&backend->mock, slot, patches, patch_count, metrics);
  if (backend->current_slot_abs_slot != slot->abs_slot) {
    mini_gnb_c_b210_slot_backend_reset_slot_mix(backend, slot->abs_slot);
  }

  for (patch_index = 0u; patch_index < patch_count; ++patch_index) {
    const mini_gnb_c_tx_grid_patch_t* patch = &patches[patch_index];
    size_t sample_index = 0u;

    if (patch->sample_count > backend->current_slot_sample_count) {
      backend->current_slot_sample_count = patch->sample_count;
      if (backend->current_slot_sample_count > MINI_GNB_C_MAX_IQ_SAMPLES) {
        backend->current_slot_sample_count = MINI_GNB_C_MAX_IQ_SAMPLES;
      }
    }
    for (sample_index = 0u; sample_index < patch->sample_count && sample_index < MINI_GNB_C_MAX_IQ_SAMPLES;
         ++sample_index) {
      backend->current_slot_mix[sample_index].real += patch->samples[sample_index].real;
      backend->current_slot_mix[sample_index].imag += patch->samples[sample_index].imag;
    }
  }
}

void mini_gnb_c_b210_slot_backend_submit_pdcch(mini_gnb_c_b210_slot_backend_t* backend,
                                               const mini_gnb_c_slot_indication_t* slot,
                                               const mini_gnb_c_pdcch_dci_t* pdcch,
                                               struct mini_gnb_c_metrics_trace* metrics) {
  if (backend == NULL) {
    return;
  }
  mini_gnb_c_mock_radio_frontend_submit_pdcch(&backend->mock, slot, pdcch, metrics);
}

void mini_gnb_c_b210_slot_backend_finalize_slot(mini_gnb_c_b210_slot_backend_t* backend,
                                                const mini_gnb_c_slot_indication_t* slot,
                                                struct mini_gnb_c_metrics_trace* metrics) {
  size_t channel_index = 0u;
  size_t sample_index = 0u;
  uint32_t flags = MINI_GNB_C_SC16_RING_FLAG_HOST_TIME_FALLBACK;

  if (backend == NULL || slot == NULL) {
    return;
  }

  mini_gnb_c_mock_radio_frontend_finalize_slot(&backend->mock, slot, metrics);

  if (backend->current_slot_abs_slot != slot->abs_slot) {
    mini_gnb_c_b210_slot_backend_reset_slot_mix(backend, slot->abs_slot);
  }
  memset(backend->tx_slot_sc16, 0, backend->tx_slot_sc16_capacity_samples * backend->channel_count * 2u * sizeof(int16_t));
  for (channel_index = 0u; channel_index < backend->channel_count; ++channel_index) {
    int16_t* channel_base = backend->tx_slot_sc16 + ((size_t)channel_index * backend->tx_slot_sc16_capacity_samples * 2u);

    for (sample_index = 0u; sample_index < backend->current_slot_sample_count && sample_index < MINI_GNB_C_MAX_IQ_SAMPLES;
         ++sample_index) {
      channel_base[sample_index * 2u] = mini_gnb_c_b210_slot_backend_float_to_sc16(backend->current_slot_mix[sample_index].real);
      channel_base[sample_index * 2u + 1u] =
          mini_gnb_c_b210_slot_backend_float_to_sc16(backend->current_slot_mix[sample_index].imag);
    }
  }
  if (mini_gnb_c_sc16_ring_map_append(&backend->tx_ring,
                                      backend->tx_slot_sc16,
                                      (uint32_t)backend->ring_block_samples,
                                      (uint64_t)(slot->slot_start_ns >= 0 ? slot->slot_start_ns : 0),
                                      flags,
                                      backend->last_error,
                                      sizeof(backend->last_error)) == 0) {
    backend->tx_ring_blocks_committed += 1u;
  }
  mini_gnb_c_b210_slot_backend_reset_slot_mix(backend, slot->abs_slot + 1);
}

void mini_gnb_c_b210_slot_backend_destroy(mini_gnb_c_b210_slot_backend_t** backend_ptr) {
  mini_gnb_c_b210_slot_backend_t* backend = NULL;

  if (backend_ptr == NULL || *backend_ptr == NULL) {
    return;
  }
  backend = *backend_ptr;
  backend->stop_requested = true;

#ifdef MINI_GNB_C_HAVE_UHD
  if (backend->rx_streamer != NULL) {
    uhd_stream_cmd_t stream_cmd;

    memset(&stream_cmd, 0, sizeof(stream_cmd));
    stream_cmd.stream_mode = UHD_STREAM_MODE_STOP_CONTINUOUS;
    stream_cmd.stream_now = false;
    (void)uhd_rx_streamer_issue_stream_cmd(backend->rx_streamer, &stream_cmd);
  }
  if (backend->rx_thread_started) {
    (void)pthread_join(backend->rx_thread, NULL);
  }
  if (backend->tx_thread_started) {
    (void)pthread_join(backend->tx_thread, NULL);
  }
  mini_gnb_c_sc16_ring_map_close(&backend->rx_ring);
  mini_gnb_c_sc16_ring_map_close(&backend->tx_ring);
  if (backend->tx_streamer != NULL) {
    uhd_tx_streamer_free(&backend->tx_streamer);
  }
  if (backend->rx_streamer != NULL) {
    uhd_rx_streamer_free(&backend->rx_streamer);
  }
  if (backend->usrp != NULL) {
    uhd_usrp_free(&backend->usrp);
  }
#endif

  if (backend->tx_slot_sc16 != NULL) {
    free(backend->tx_slot_sc16);
  }
  mini_gnb_c_mock_radio_frontend_shutdown(&backend->mock);
  free(backend);
  *backend_ptr = NULL;
}
