#ifndef MINI_GNB_C_RADIO_MOCK_RADIO_FRONTEND_H
#define MINI_GNB_C_RADIO_MOCK_RADIO_FRONTEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mini_gnb_c/common/types.h"
#include "mini_gnb_c/link/shared_slot_link.h"

struct mini_gnb_c_metrics_trace;

typedef struct {
  mini_gnb_c_rf_config_t config;
  mini_gnb_c_sim_config_t sim;
  uint64_t tx_burst_count;
  int64_t last_hw_time_ns;
  bool slot_input_mode_enabled;
  bool local_exchange_mode_enabled;
  bool shared_slot_mode_enabled;
  uint32_t local_exchange_next_sequence;
  uint32_t shared_slot_timeout_ms;
  int shared_slot_summary_abs_slot;
  bool initial_prach_emitted;
  bool retry_prach_armed;
  int retry_prach_abs_slot;
  bool msg3_armed;
  int msg3_abs_slot;
  uint16_t msg3_rnti;
  bool pucch_sr_armed;
  int pucch_sr_abs_slot;
  uint16_t pucch_sr_rnti;
  bool ul_data_armed;
  int ul_data_abs_slot;
  uint16_t ul_data_rnti;
  uint16_t ul_data_tbsize;
  mini_gnb_c_ul_data_purpose_t ul_data_purpose;
  bool harq_dl_ack_armed[MINI_GNB_C_MAX_HARQ_PROCESSES];
  int harq_dl_ack_abs_slot[MINI_GNB_C_MAX_HARQ_PROCESSES];
  uint16_t harq_dl_ack_rnti[MINI_GNB_C_MAX_HARQ_PROCESSES];
  bool harq_ul_data_armed[MINI_GNB_C_MAX_HARQ_PROCESSES];
  int harq_ul_data_abs_slot[MINI_GNB_C_MAX_HARQ_PROCESSES];
  uint16_t harq_ul_data_rnti[MINI_GNB_C_MAX_HARQ_PROCESSES];
  uint16_t harq_ul_data_tbsize[MINI_GNB_C_MAX_HARQ_PROCESSES];
  mini_gnb_c_ul_data_purpose_t harq_ul_data_purpose[MINI_GNB_C_MAX_HARQ_PROCESSES];
  bool harq_ul_data_ndi[MINI_GNB_C_MAX_HARQ_PROCESSES];
  bool harq_ul_data_is_new_data[MINI_GNB_C_MAX_HARQ_PROCESSES];
  size_t prach_sample_count;
  size_t msg3_sample_count;
  size_t pucch_sr_sample_count;
  size_t ul_data_sample_count;
  mini_gnb_c_buffer_t msg3_mac_pdu;
  mini_gnb_c_buffer_t ul_data_mac_pdu;
  mini_gnb_c_complexf_t prach_samples[MINI_GNB_C_MAX_IQ_SAMPLES];
  mini_gnb_c_complexf_t msg3_samples[MINI_GNB_C_MAX_IQ_SAMPLES];
  mini_gnb_c_complexf_t pucch_sr_samples[MINI_GNB_C_MAX_IQ_SAMPLES];
  mini_gnb_c_complexf_t ul_data_samples[MINI_GNB_C_MAX_IQ_SAMPLES];
  mini_gnb_c_shared_slot_link_t shared_slot_link;
  mini_gnb_c_shared_slot_dl_summary_t shared_slot_summary;
  bool staged_ue_ipv4_valid;
  uint8_t staged_ue_ipv4[4];
} mini_gnb_c_mock_radio_frontend_t;

void mini_gnb_c_mock_radio_frontend_init(mini_gnb_c_mock_radio_frontend_t* radio,
                                         const mini_gnb_c_rf_config_t* rf_config,
                                         const mini_gnb_c_sim_config_t* sim_config);

void mini_gnb_c_mock_radio_frontend_receive(mini_gnb_c_mock_radio_frontend_t* radio,
                                            const mini_gnb_c_slot_indication_t* slot,
                                            mini_gnb_c_radio_burst_t* out_burst);

void mini_gnb_c_mock_radio_frontend_arm_msg3(mini_gnb_c_mock_radio_frontend_t* radio,
                                             const mini_gnb_c_ul_grant_for_msg3_t* ul_grant);

void mini_gnb_c_mock_radio_frontend_arm_pucch_sr(mini_gnb_c_mock_radio_frontend_t* radio,
                                                 uint16_t rnti,
                                                 int abs_slot);

void mini_gnb_c_mock_radio_frontend_arm_dl_ack(mini_gnb_c_mock_radio_frontend_t* radio,
                                               uint16_t rnti,
                                               uint8_t harq_id,
                                               int abs_slot);

void mini_gnb_c_mock_radio_frontend_arm_ul_data(mini_gnb_c_mock_radio_frontend_t* radio,
                                                const mini_gnb_c_ul_data_grant_t* ul_grant);

void mini_gnb_c_mock_radio_frontend_stage_ue_ipv4(mini_gnb_c_mock_radio_frontend_t* radio,
                                                  const uint8_t ue_ipv4[4],
                                                  bool ue_ipv4_valid);

void mini_gnb_c_mock_radio_frontend_submit_tx(mini_gnb_c_mock_radio_frontend_t* radio,
                                              const mini_gnb_c_slot_indication_t* slot,
                                              const mini_gnb_c_tx_grid_patch_t* patches,
                                              size_t patch_count,
                                              struct mini_gnb_c_metrics_trace* metrics);

void mini_gnb_c_mock_radio_frontend_submit_pdcch(mini_gnb_c_mock_radio_frontend_t* radio,
                                                 const mini_gnb_c_slot_indication_t* slot,
                                                 const mini_gnb_c_pdcch_dci_t* pdcch,
                                                 struct mini_gnb_c_metrics_trace* metrics);

void mini_gnb_c_mock_radio_frontend_finalize_slot(mini_gnb_c_mock_radio_frontend_t* radio,
                                                  const mini_gnb_c_slot_indication_t* slot,
                                                  struct mini_gnb_c_metrics_trace* metrics);

void mini_gnb_c_mock_radio_frontend_shutdown(mini_gnb_c_mock_radio_frontend_t* radio);

#endif
