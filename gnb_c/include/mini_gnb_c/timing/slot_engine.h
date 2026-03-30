#ifndef MINI_GNB_C_TIMING_SLOT_ENGINE_H
#define MINI_GNB_C_TIMING_SLOT_ENGINE_H

#include "mini_gnb_c/common/types.h"

typedef struct {
  mini_gnb_c_config_t config;
} mini_gnb_c_slot_engine_t;

void mini_gnb_c_slot_engine_init(mini_gnb_c_slot_engine_t* engine,
                                 const mini_gnb_c_config_t* config);

void mini_gnb_c_slot_engine_make_slot(const mini_gnb_c_slot_engine_t* engine,
                                      int abs_slot,
                                      mini_gnb_c_slot_indication_t* out_slot);

#endif
