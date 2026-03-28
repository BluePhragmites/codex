#ifndef MINI_GNB_C_PHY_UL_MOCK_MSG3_RECEIVER_H
#define MINI_GNB_C_PHY_UL_MOCK_MSG3_RECEIVER_H

#include <stdbool.h>

#include "mini_gnb_c/common/types.h"

typedef struct {
  mini_gnb_c_sim_config_t config;
} mini_gnb_c_mock_msg3_receiver_t;

void mini_gnb_c_mock_msg3_receiver_init(mini_gnb_c_mock_msg3_receiver_t* receiver,
                                        const mini_gnb_c_sim_config_t* config);

bool mini_gnb_c_mock_msg3_receiver_decode(const mini_gnb_c_mock_msg3_receiver_t* receiver,
                                          const mini_gnb_c_slot_indication_t* slot,
                                          const mini_gnb_c_ul_grant_for_msg3_t* ul_grant,
                                          const mini_gnb_c_radio_burst_t* burst,
                                          mini_gnb_c_msg3_decode_indication_t* out_msg3);

#endif
