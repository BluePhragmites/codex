#include "mini_gnb_c/timing/slot_engine.h"

#include <string.h>

void mini_gnb_c_slot_engine_init(mini_gnb_c_slot_engine_t* engine,
                                 const mini_gnb_c_config_t* config) {
  if (engine == NULL || config == NULL) {
    return;
  }
  memcpy(&engine->config, config, sizeof(*config));
}

void mini_gnb_c_slot_engine_make_slot(const mini_gnb_c_slot_engine_t* engine,
                                      const int abs_slot,
                                      mini_gnb_c_slot_indication_t* out_slot) {
  if (engine == NULL || out_slot == NULL) {
    return;
  }

  memset(out_slot, 0, sizeof(*out_slot));
  out_slot->abs_slot = abs_slot;
  out_slot->slot = (uint16_t)(abs_slot % engine->config.sim.slots_per_frame);
  out_slot->sfn = (uint32_t)((abs_slot / engine->config.sim.slots_per_frame) % 1024);
  out_slot->slot_start_ns = (int64_t)abs_slot * 500000;
  out_slot->has_ssb = (abs_slot % engine->config.broadcast.ssb_period_slots) == 0;
  out_slot->has_sib1 = (abs_slot % engine->config.broadcast.sib1_period_slots) == 0;
  out_slot->has_prach_occasion = abs_slot == engine->config.sim.prach_trigger_abs_slot;
  out_slot->is_ul_slot = true;
  out_slot->is_dl_slot = true;
}
