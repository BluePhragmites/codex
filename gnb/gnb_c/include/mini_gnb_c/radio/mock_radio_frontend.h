#ifndef MINI_GNB_C_RADIO_MOCK_RADIO_FRONTEND_H
#define MINI_GNB_C_RADIO_MOCK_RADIO_FRONTEND_H

#include <stddef.h>
#include <stdint.h>

#include "mini_gnb_c/common/types.h"

struct mini_gnb_c_metrics_trace;

typedef struct {
  mini_gnb_c_rf_config_t config;
  mini_gnb_c_sim_config_t sim;
  uint64_t tx_burst_count;
  int64_t last_hw_time_ns;
  bool initial_prach_emitted;
  bool retry_prach_armed;
  int retry_prach_abs_slot;
  bool msg3_armed;
  int msg3_abs_slot;
  uint16_t msg3_rnti;
  size_t prach_sample_count;
  size_t msg3_sample_count;
  mini_gnb_c_buffer_t msg3_mac_pdu;
  mini_gnb_c_complexf_t prach_samples[MINI_GNB_C_MAX_IQ_SAMPLES];
  mini_gnb_c_complexf_t msg3_samples[MINI_GNB_C_MAX_IQ_SAMPLES];
} mini_gnb_c_mock_radio_frontend_t;

void mini_gnb_c_mock_radio_frontend_init(mini_gnb_c_mock_radio_frontend_t* radio,
                                         const mini_gnb_c_rf_config_t* rf_config,
                                         const mini_gnb_c_sim_config_t* sim_config);

void mini_gnb_c_mock_radio_frontend_receive(mini_gnb_c_mock_radio_frontend_t* radio,
                                            const mini_gnb_c_slot_indication_t* slot,
                                            mini_gnb_c_radio_burst_t* out_burst);

void mini_gnb_c_mock_radio_frontend_arm_msg3(mini_gnb_c_mock_radio_frontend_t* radio,
                                             const mini_gnb_c_ul_grant_for_msg3_t* ul_grant);

void mini_gnb_c_mock_radio_frontend_submit_tx(mini_gnb_c_mock_radio_frontend_t* radio,
                                              const mini_gnb_c_slot_indication_t* slot,
                                              const mini_gnb_c_tx_grid_patch_t* patches,
                                              size_t patch_count,
                                              struct mini_gnb_c_metrics_trace* metrics);

#endif
