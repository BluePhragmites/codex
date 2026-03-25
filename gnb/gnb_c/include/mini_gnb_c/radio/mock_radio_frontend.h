#ifndef MINI_GNB_C_RADIO_MOCK_RADIO_FRONTEND_H
#define MINI_GNB_C_RADIO_MOCK_RADIO_FRONTEND_H

#include <stddef.h>
#include <stdint.h>

#include "mini_gnb_c/common/types.h"

struct mini_gnb_c_metrics_trace;

typedef struct {
  mini_gnb_c_rf_config_t config;
  uint64_t tx_burst_count;
  int64_t last_hw_time_ns;
} mini_gnb_c_mock_radio_frontend_t;

void mini_gnb_c_mock_radio_frontend_init(mini_gnb_c_mock_radio_frontend_t* radio,
                                         const mini_gnb_c_rf_config_t* config);

void mini_gnb_c_mock_radio_frontend_receive(mini_gnb_c_mock_radio_frontend_t* radio,
                                            const mini_gnb_c_slot_indication_t* slot,
                                            mini_gnb_c_radio_burst_t* out_burst);

void mini_gnb_c_mock_radio_frontend_submit_tx(mini_gnb_c_mock_radio_frontend_t* radio,
                                              const mini_gnb_c_slot_indication_t* slot,
                                              const mini_gnb_c_tx_grid_patch_t* patches,
                                              size_t patch_count,
                                              struct mini_gnb_c_metrics_trace* metrics);

#endif
