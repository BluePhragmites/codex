#ifndef MINI_GNB_C_BROADCAST_BROADCAST_ENGINE_H
#define MINI_GNB_C_BROADCAST_BROADCAST_ENGINE_H

#include <stddef.h>

#include "mini_gnb_c/common/types.h"

typedef struct {
  mini_gnb_c_cell_config_t cell;
  mini_gnb_c_prach_config_t prach;
  mini_gnb_c_broadcast_config_t broadcast;
  mini_gnb_c_sim_config_t sim;
} mini_gnb_c_broadcast_engine_t;

void mini_gnb_c_broadcast_engine_init(mini_gnb_c_broadcast_engine_t* engine,
                                      const mini_gnb_c_cell_config_t* cell,
                                      const mini_gnb_c_prach_config_t* prach,
                                      const mini_gnb_c_broadcast_config_t* broadcast,
                                      const mini_gnb_c_sim_config_t* sim);

size_t mini_gnb_c_broadcast_schedule(const mini_gnb_c_broadcast_engine_t* engine,
                                     const mini_gnb_c_slot_indication_t* slot,
                                     mini_gnb_c_dl_grant_t* out_grants,
                                     size_t max_grants);

#endif
