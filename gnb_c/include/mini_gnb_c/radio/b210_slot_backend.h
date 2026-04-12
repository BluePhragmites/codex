#ifndef MINI_GNB_C_RADIO_B210_SLOT_BACKEND_H
#define MINI_GNB_C_RADIO_B210_SLOT_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mini_gnb_c/common/types.h"

struct mini_gnb_c_metrics_trace;
struct mini_gnb_c_b210_slot_backend;

typedef struct mini_gnb_c_b210_slot_backend mini_gnb_c_b210_slot_backend_t;

int mini_gnb_c_b210_slot_backend_create(mini_gnb_c_b210_slot_backend_t** out_backend,
                                        const mini_gnb_c_rf_config_t* rf_config,
                                        const mini_gnb_c_sim_config_t* sim_config,
                                        char* error_message,
                                        size_t error_message_size);

const char* mini_gnb_c_b210_slot_backend_error(const mini_gnb_c_b210_slot_backend_t* backend);

bool mini_gnb_c_b210_slot_backend_has_pucch_sr_armed_for(const mini_gnb_c_b210_slot_backend_t* backend,
                                                         uint16_t rnti,
                                                         int abs_slot,
                                                         int current_abs_slot);

uint64_t mini_gnb_c_b210_slot_backend_tx_burst_count(const mini_gnb_c_b210_slot_backend_t* backend);

int64_t mini_gnb_c_b210_slot_backend_last_hw_time_ns(const mini_gnb_c_b210_slot_backend_t* backend);

void mini_gnb_c_b210_slot_backend_receive(mini_gnb_c_b210_slot_backend_t* backend,
                                          const mini_gnb_c_slot_indication_t* slot,
                                          mini_gnb_c_radio_burst_t* out_burst);

void mini_gnb_c_b210_slot_backend_arm_msg3(mini_gnb_c_b210_slot_backend_t* backend,
                                           const mini_gnb_c_ul_grant_for_msg3_t* ul_grant);

void mini_gnb_c_b210_slot_backend_arm_pucch_sr(mini_gnb_c_b210_slot_backend_t* backend,
                                               uint16_t rnti,
                                               int abs_slot);

void mini_gnb_c_b210_slot_backend_arm_dl_ack(mini_gnb_c_b210_slot_backend_t* backend,
                                             uint16_t rnti,
                                             uint8_t harq_id,
                                             int abs_slot);

void mini_gnb_c_b210_slot_backend_arm_ul_data(mini_gnb_c_b210_slot_backend_t* backend,
                                              const mini_gnb_c_ul_data_grant_t* ul_grant);

void mini_gnb_c_b210_slot_backend_stage_ue_ipv4(mini_gnb_c_b210_slot_backend_t* backend,
                                                const uint8_t ue_ipv4[4],
                                                bool ue_ipv4_valid);

void mini_gnb_c_b210_slot_backend_submit_tx(mini_gnb_c_b210_slot_backend_t* backend,
                                            const mini_gnb_c_slot_indication_t* slot,
                                            const mini_gnb_c_tx_grid_patch_t* patches,
                                            size_t patch_count,
                                            struct mini_gnb_c_metrics_trace* metrics);

void mini_gnb_c_b210_slot_backend_submit_pdcch(mini_gnb_c_b210_slot_backend_t* backend,
                                               const mini_gnb_c_slot_indication_t* slot,
                                               const mini_gnb_c_pdcch_dci_t* pdcch,
                                               struct mini_gnb_c_metrics_trace* metrics);

void mini_gnb_c_b210_slot_backend_finalize_slot(mini_gnb_c_b210_slot_backend_t* backend,
                                                const mini_gnb_c_slot_indication_t* slot,
                                                struct mini_gnb_c_metrics_trace* metrics);

void mini_gnb_c_b210_slot_backend_destroy(mini_gnb_c_b210_slot_backend_t** backend_ptr);

#endif
