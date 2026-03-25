#ifndef MINI_GNB_C_PHY_UL_MOCK_PRACH_DETECTOR_H
#define MINI_GNB_C_PHY_UL_MOCK_PRACH_DETECTOR_H

#include <stdbool.h>

#include "mini_gnb_c/common/types.h"

typedef struct {
  mini_gnb_c_sim_config_t config;
  bool fired;
} mini_gnb_c_mock_prach_detector_t;

void mini_gnb_c_mock_prach_detector_init(mini_gnb_c_mock_prach_detector_t* detector,
                                         const mini_gnb_c_sim_config_t* config);

bool mini_gnb_c_mock_prach_detector_detect(mini_gnb_c_mock_prach_detector_t* detector,
                                           const mini_gnb_c_slot_indication_t* slot,
                                           const mini_gnb_c_radio_burst_t* burst,
                                           mini_gnb_c_prach_indication_t* out_prach);

#endif
