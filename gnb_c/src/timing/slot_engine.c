#include "mini_gnb_c/timing/slot_engine.h"

#include <string.h>

static bool mini_gnb_c_is_periodic_slot(const int abs_slot, const int period_slots, const int offset_slot) {
  if (period_slots <= 0 || offset_slot < 0 || abs_slot < offset_slot) {
    return false;
  }
  return ((abs_slot - offset_slot) % period_slots) == 0;
}

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
  out_slot->has_ssb = mini_gnb_c_is_periodic_slot(abs_slot, engine->config.broadcast.ssb_period_slots, 0);
  out_slot->has_sib1 = mini_gnb_c_is_periodic_slot(abs_slot,
                                                   engine->config.broadcast.sib1_period_slots,
                                                   engine->config.broadcast.sib1_offset_slot);
  out_slot->is_ul_slot = true;
  out_slot->is_dl_slot = true;
  out_slot->has_prach_occasion = out_slot->is_ul_slot &&
                                 mini_gnb_c_is_periodic_slot(abs_slot,
                                                             engine->config.broadcast.prach_period_slots,
                                                             engine->config.broadcast.prach_offset_slot);
}
