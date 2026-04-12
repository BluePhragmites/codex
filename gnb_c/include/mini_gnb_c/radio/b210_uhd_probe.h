#ifndef MINI_GNB_C_RADIO_B210_UHD_PROBE_H
#define MINI_GNB_C_RADIO_B210_UHD_PROBE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "mini_gnb_c/common/types.h"

typedef enum {
  MINI_GNB_C_B210_DURATION_MODE_SAMPLE_TARGET = 0,
  MINI_GNB_C_B210_DURATION_MODE_WALLCLOCK = 1
} mini_gnb_c_b210_duration_mode_t;

typedef struct {
  char device_args[128];
  char subdev[32];
  char ref[16];
  double rate_sps;
  double freq_hz;
  double gain_db;
  double bandwidth_hz;
  double duration_sec;
  mini_gnb_c_b210_duration_mode_t duration_mode;
  size_t channel;
  uint32_t channel_count;
  int cpu_core;
  bool apply_host_tuning;
  bool require_ref_lock;
  bool require_lo_lock;
  char output_path[MINI_GNB_C_MAX_PATH];
  char ring_path[MINI_GNB_C_MAX_PATH];
  uint32_t ring_block_samples;
  uint32_t ring_block_count;
} mini_gnb_c_b210_probe_config_t;

typedef struct {
  char device_summary[1024];
  char rx_subdev_name[64];
  double actual_rate_sps;
  double actual_freq_hz;
  double actual_gain_db;
  double actual_bandwidth_hz;
  bool ref_locked_valid;
  bool ref_locked;
  bool lo_locked_valid;
  bool lo_locked;
  char host_tuning_summary[256];
  size_t requested_samples;
  size_t received_samples;
  uint32_t channel_count;
  bool used_ring_map;
  size_t ring_blocks_committed;
  size_t rx_overflow_events;
  size_t rx_timeout_events;
  size_t rx_recoverable_events;
  size_t rx_gap_events;
  size_t rx_lost_samples_estimate;
  double wall_elapsed_sec;
  mini_gnb_c_b210_duration_mode_t duration_mode;
} mini_gnb_c_b210_probe_report_t;

typedef struct {
  char device_args[128];
  char subdev[32];
  char ref[16];
  double rate_sps;
  double freq_hz;
  double gain_db;
  double bandwidth_hz;
  size_t channel;
  uint32_t channel_count;
  int cpu_core;
  bool apply_host_tuning;
  bool require_ref_lock;
  bool require_lo_lock;
  char input_path[MINI_GNB_C_MAX_PATH];
  char ring_path[MINI_GNB_C_MAX_PATH];
  uint32_t ring_block_samples;
  uint32_t ring_block_count;
  uint32_t tx_prefetch_samples;
} mini_gnb_c_b210_tx_config_t;

typedef struct {
  char device_summary[1024];
  char tx_subdev_name[64];
  double actual_rate_sps;
  double actual_freq_hz;
  double actual_gain_db;
  double actual_bandwidth_hz;
  bool ref_locked_valid;
  bool ref_locked;
  bool lo_locked_valid;
  bool lo_locked;
  char host_tuning_summary[256];
  size_t requested_samples;
  size_t transmitted_samples;
  bool burst_ack_valid;
  bool burst_ack;
  bool underflow_observed;
  bool seq_error_observed;
  bool time_error_observed;
  uint32_t channel_count;
  bool used_ring_map;
  size_t ring_blocks_committed;
  size_t tx_prefetch_samples;
} mini_gnb_c_b210_tx_report_t;

typedef struct {
  char device_args[128];
  char subdev[32];
  char ref[16];
  double rate_sps;
  double rx_freq_hz;
  double tx_freq_hz;
  double rx_gain_db;
  double tx_gain_db;
  double bandwidth_hz;
  double duration_sec;
  mini_gnb_c_b210_duration_mode_t duration_mode;
  size_t channel;
  uint32_t channel_count;
  int rx_cpu_core;
  int tx_cpu_core;
  bool apply_host_tuning;
  bool require_ref_lock;
  bool require_lo_lock;
  char rx_output_path[MINI_GNB_C_MAX_PATH];
  char tx_input_path[MINI_GNB_C_MAX_PATH];
  char rx_ring_path[MINI_GNB_C_MAX_PATH];
  char tx_ring_path[MINI_GNB_C_MAX_PATH];
  uint32_t ring_block_samples;
  uint32_t ring_block_count;
  uint32_t tx_prefetch_samples;
} mini_gnb_c_b210_trx_config_t;

typedef struct {
  char device_summary[1024];
  char host_tuning_summary[256];
  double actual_rate_sps;
  double actual_rx_freq_hz;
  double actual_tx_freq_hz;
  double actual_rx_gain_db;
  double actual_tx_gain_db;
  double actual_bandwidth_hz;
  bool ref_locked_valid;
  bool ref_locked;
  bool rx_lo_locked_valid;
  bool rx_lo_locked;
  bool tx_lo_locked_valid;
  bool tx_lo_locked;
  size_t requested_samples;
  size_t received_samples;
  size_t transmitted_samples;
  uint32_t channel_count;
  bool used_rx_ring_map;
  bool used_tx_ring_map;
  size_t rx_ring_blocks_committed;
  size_t tx_ring_blocks_committed;
  size_t tx_prefetch_samples;
  size_t tx_ring_wrap_count;
  size_t rx_overflow_events;
  size_t rx_timeout_events;
  size_t rx_recoverable_events;
  size_t rx_gap_events;
  size_t rx_lost_samples_estimate;
  double wall_elapsed_sec;
  mini_gnb_c_b210_duration_mode_t duration_mode;
  bool burst_ack_valid;
  bool burst_ack;
  bool underflow_observed;
  bool seq_error_observed;
  bool time_error_observed;
} mini_gnb_c_b210_trx_report_t;

static inline void mini_gnb_c_b210_probe_config_init(mini_gnb_c_b210_probe_config_t* config) {
  if (config == NULL) {
    return;
  }
  config->device_args[0] = '\0';
  config->subdev[0] = '\0';
  (void)snprintf(config->ref, sizeof(config->ref), "%s", "external");
  config->rate_sps = 20e6;
  config->freq_hz = 2462e6;
  config->gain_db = 60.0;
  config->bandwidth_hz = 20e6;
  config->duration_sec = 1.0;
  config->duration_mode = MINI_GNB_C_B210_DURATION_MODE_SAMPLE_TARGET;
  config->channel = 0u;
  config->channel_count = 1u;
  config->cpu_core = -1;
  config->apply_host_tuning = true;
  config->require_ref_lock = true;
  config->require_lo_lock = true;
  (void)snprintf(config->output_path, sizeof(config->output_path), "%s", "/dev/shm/b210_probe_rx_fc32.dat");
  config->ring_path[0] = '\0';
  config->ring_block_samples = 4096u;
  config->ring_block_count = 1024u;
}

static inline void mini_gnb_c_b210_apply_shared_gain(mini_gnb_c_b210_probe_config_t* rx_config,
                                                     mini_gnb_c_b210_tx_config_t* tx_config,
                                                     mini_gnb_c_b210_trx_config_t* trx_config,
                                                     const double gain_db) {
  if (rx_config != NULL) {
    rx_config->gain_db = gain_db;
  }
  if (tx_config != NULL) {
    tx_config->gain_db = gain_db;
  }
  if (trx_config != NULL) {
    trx_config->rx_gain_db = gain_db;
    trx_config->tx_gain_db = gain_db;
  }
}

static inline void mini_gnb_c_b210_apply_shared_freq(mini_gnb_c_b210_probe_config_t* rx_config,
                                                     mini_gnb_c_b210_tx_config_t* tx_config,
                                                     mini_gnb_c_b210_trx_config_t* trx_config,
                                                     const double freq_hz) {
  if (rx_config != NULL) {
    rx_config->freq_hz = freq_hz;
  }
  if (tx_config != NULL) {
    tx_config->freq_hz = freq_hz;
  }
  if (trx_config != NULL) {
    trx_config->rx_freq_hz = freq_hz;
    trx_config->tx_freq_hz = freq_hz;
  }
}

int mini_gnb_c_b210_probe_run(const mini_gnb_c_b210_probe_config_t* config,
                              mini_gnb_c_b210_probe_report_t* report,
                              char* error_message,
                              size_t error_message_size);

static inline void mini_gnb_c_b210_tx_config_init(mini_gnb_c_b210_tx_config_t* config) {
  if (config == NULL) {
    return;
  }
  config->device_args[0] = '\0';
  config->subdev[0] = '\0';
  (void)snprintf(config->ref, sizeof(config->ref), "%s", "external");
  config->rate_sps = 20e6;
  config->freq_hz = 2462e6;
  config->gain_db = 60.0;
  config->bandwidth_hz = 20e6;
  config->channel = 0u;
  config->channel_count = 1u;
  config->cpu_core = -1;
  config->apply_host_tuning = true;
  config->require_ref_lock = true;
  config->require_lo_lock = true;
  (void)snprintf(config->input_path, sizeof(config->input_path), "%s", "/dev/shm/test.dat");
  config->ring_path[0] = '\0';
  config->ring_block_samples = 4096u;
  config->ring_block_count = 1024u;
  config->tx_prefetch_samples = 0u;
}

static inline void mini_gnb_c_b210_apply_rx_gain(mini_gnb_c_b210_probe_config_t* rx_config,
                                                 mini_gnb_c_b210_trx_config_t* trx_config,
                                                 const double gain_db) {
  if (rx_config != NULL) {
    rx_config->gain_db = gain_db;
  }
  if (trx_config != NULL) {
    trx_config->rx_gain_db = gain_db;
  }
}

static inline void mini_gnb_c_b210_apply_rx_freq(mini_gnb_c_b210_probe_config_t* rx_config,
                                                 mini_gnb_c_b210_trx_config_t* trx_config,
                                                 const double freq_hz) {
  if (rx_config != NULL) {
    rx_config->freq_hz = freq_hz;
  }
  if (trx_config != NULL) {
    trx_config->rx_freq_hz = freq_hz;
  }
}

static inline void mini_gnb_c_b210_apply_tx_gain(mini_gnb_c_b210_tx_config_t* tx_config,
                                                 mini_gnb_c_b210_trx_config_t* trx_config,
                                                 const double gain_db) {
  if (tx_config != NULL) {
    tx_config->gain_db = gain_db;
  }
  if (trx_config != NULL) {
    trx_config->tx_gain_db = gain_db;
  }
}

static inline void mini_gnb_c_b210_apply_tx_freq(mini_gnb_c_b210_tx_config_t* tx_config,
                                                 mini_gnb_c_b210_trx_config_t* trx_config,
                                                 const double freq_hz) {
  if (tx_config != NULL) {
    tx_config->freq_hz = freq_hz;
  }
  if (trx_config != NULL) {
    trx_config->tx_freq_hz = freq_hz;
  }
}

static inline uint64_t mini_gnb_c_b210_time_spec_to_ns(const int64_t full_secs, const double frac_secs) {
  uint64_t whole_ns = 0u;
  double frac_ns = frac_secs * 1000000000.0;
  uint64_t frac_ns_rounded = 0u;

  if (full_secs <= 0 && frac_secs <= 0.0) {
    return 0u;
  }
  if (full_secs > 0) {
    whole_ns = (uint64_t)full_secs * 1000000000ULL;
  }
  if (frac_ns > 0.0) {
    frac_ns_rounded = (uint64_t)(frac_ns + 0.5);
  }
  if (frac_ns_rounded >= 1000000000ULL) {
    whole_ns += 1000000000ULL;
    frac_ns_rounded -= 1000000000ULL;
  }
  return whole_ns + frac_ns_rounded;
}

static inline uint64_t mini_gnb_c_b210_advance_time_ns(const uint64_t start_ns,
                                                       const size_t sample_offset,
                                                       const double rate_sps) {
  double delta_ns = 0.0;

  if (rate_sps <= 0.0 || sample_offset == 0u) {
    return start_ns;
  }
  delta_ns = ((double)sample_offset * 1000000000.0) / rate_sps;
  return start_ns + (uint64_t)(delta_ns + 0.5);
}

int mini_gnb_c_b210_tx_from_file_run(const mini_gnb_c_b210_tx_config_t* config,
                                     mini_gnb_c_b210_tx_report_t* report,
                                     char* error_message,
                                     size_t error_message_size);

static inline void mini_gnb_c_b210_trx_config_init(mini_gnb_c_b210_trx_config_t* config) {
  if (config == NULL) {
    return;
  }
  config->device_args[0] = '\0';
  config->subdev[0] = '\0';
  (void)snprintf(config->ref, sizeof(config->ref), "%s", "external");
  config->rate_sps = 20e6;
  config->rx_freq_hz = 2462e6;
  config->tx_freq_hz = 2462e6;
  config->rx_gain_db = 60.0;
  config->tx_gain_db = 60.0;
  config->bandwidth_hz = 20e6;
  config->duration_sec = 1.0;
  config->duration_mode = MINI_GNB_C_B210_DURATION_MODE_SAMPLE_TARGET;
  config->channel = 0u;
  config->channel_count = 1u;
  config->rx_cpu_core = -1;
  config->tx_cpu_core = -1;
  config->apply_host_tuning = true;
  config->require_ref_lock = true;
  config->require_lo_lock = true;
  (void)snprintf(config->rx_output_path, sizeof(config->rx_output_path), "%s", "/dev/shm/b210_trx_rx_fc32.dat");
  (void)snprintf(config->tx_input_path, sizeof(config->tx_input_path), "%s", "/dev/shm/test.dat");
  config->rx_ring_path[0] = '\0';
  config->tx_ring_path[0] = '\0';
  config->ring_block_samples = 4096u;
  config->ring_block_count = 1024u;
  config->tx_prefetch_samples = 0u;
}

int mini_gnb_c_b210_trx_run(const mini_gnb_c_b210_trx_config_t* config,
                            mini_gnb_c_b210_trx_report_t* report,
                            char* error_message,
                            size_t error_message_size);

#endif
