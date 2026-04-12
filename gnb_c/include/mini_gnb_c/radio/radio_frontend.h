#ifndef MINI_GNB_C_RADIO_RADIO_FRONTEND_H
#define MINI_GNB_C_RADIO_RADIO_FRONTEND_H

#include <stdbool.h>

#include "mini_gnb_c/common/types.h"
#include "mini_gnb_c/radio/mock_radio_frontend.h"

struct mini_gnb_c_metrics_trace;
typedef struct mini_gnb_c_b210_slot_backend mini_gnb_c_b210_slot_backend_t;

typedef struct {
  mini_gnb_c_radio_backend_kind_t kind;
  bool ready;
  char driver_name[16];
  char last_error[128];
  mini_gnb_c_mock_radio_frontend_t mock;
  mini_gnb_c_b210_slot_backend_t* b210;
} mini_gnb_c_radio_frontend_t;

int mini_gnb_c_radio_frontend_init(mini_gnb_c_radio_frontend_t* radio,
                                   const mini_gnb_c_rf_config_t* rf_config,
                                   const mini_gnb_c_sim_config_t* sim_config);

bool mini_gnb_c_radio_frontend_is_ready(const mini_gnb_c_radio_frontend_t* radio);

mini_gnb_c_radio_backend_kind_t mini_gnb_c_radio_frontend_kind(const mini_gnb_c_radio_frontend_t* radio);

const char* mini_gnb_c_radio_frontend_driver_name(const mini_gnb_c_radio_frontend_t* radio);

const char* mini_gnb_c_radio_frontend_error(const mini_gnb_c_radio_frontend_t* radio);

bool mini_gnb_c_radio_frontend_has_pucch_sr_armed_for(const mini_gnb_c_radio_frontend_t* radio,
                                                      uint16_t rnti,
                                                      int abs_slot,
                                                      int current_abs_slot);

uint64_t mini_gnb_c_radio_frontend_tx_burst_count(const mini_gnb_c_radio_frontend_t* radio);

int64_t mini_gnb_c_radio_frontend_last_hw_time_ns(const mini_gnb_c_radio_frontend_t* radio);

void mini_gnb_c_radio_frontend_receive(mini_gnb_c_radio_frontend_t* radio,
                                       const mini_gnb_c_slot_indication_t* slot,
                                       mini_gnb_c_radio_burst_t* out_burst);

void mini_gnb_c_radio_frontend_arm_msg3(mini_gnb_c_radio_frontend_t* radio,
                                        const mini_gnb_c_ul_grant_for_msg3_t* ul_grant);

void mini_gnb_c_radio_frontend_arm_pucch_sr(mini_gnb_c_radio_frontend_t* radio, uint16_t rnti, int abs_slot);

void mini_gnb_c_radio_frontend_arm_dl_ack(mini_gnb_c_radio_frontend_t* radio,
                                          uint16_t rnti,
                                          uint8_t harq_id,
                                          int abs_slot);

void mini_gnb_c_radio_frontend_arm_ul_data(mini_gnb_c_radio_frontend_t* radio,
                                           const mini_gnb_c_ul_data_grant_t* ul_grant);

void mini_gnb_c_radio_frontend_stage_ue_ipv4(mini_gnb_c_radio_frontend_t* radio,
                                             const uint8_t ue_ipv4[4],
                                             bool ue_ipv4_valid);

void mini_gnb_c_radio_frontend_submit_tx(mini_gnb_c_radio_frontend_t* radio,
                                         const mini_gnb_c_slot_indication_t* slot,
                                         const mini_gnb_c_tx_grid_patch_t* patches,
                                         size_t patch_count,
                                         struct mini_gnb_c_metrics_trace* metrics);

void mini_gnb_c_radio_frontend_submit_pdcch(mini_gnb_c_radio_frontend_t* radio,
                                            const mini_gnb_c_slot_indication_t* slot,
                                            const mini_gnb_c_pdcch_dci_t* pdcch,
                                            struct mini_gnb_c_metrics_trace* metrics);

void mini_gnb_c_radio_frontend_finalize_slot(mini_gnb_c_radio_frontend_t* radio,
                                             const mini_gnb_c_slot_indication_t* slot,
                                             struct mini_gnb_c_metrics_trace* metrics);

void mini_gnb_c_radio_frontend_shutdown(mini_gnb_c_radio_frontend_t* radio);

#endif
