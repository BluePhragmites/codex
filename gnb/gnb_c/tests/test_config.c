#include <string.h>

#include "test_helpers.h"

#include "mini_gnb_c/config/config_loader.h"
#include "mini_gnb_c/timing/slot_engine.h"

void test_config_loads(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  mini_gnb_c_config_t config;

  mini_gnb_c_default_config_path(config_path, sizeof(config_path));
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) == 0,
                     "expected config to load");
  mini_gnb_c_require(config.cell.pci == 1U, "expected pci=1");
  mini_gnb_c_require(config.cell.band == 78U, "expected band n78");
  mini_gnb_c_require(config.prach.ra_resp_window == 4U, "expected ra response window");
  mini_gnb_c_require(strcmp(config.rf.device_driver, "mock") == 0, "expected mock radio frontend");
  mini_gnb_c_require(config.sim.total_slots == 18, "expected total slot count");
  mini_gnb_c_require(config.sim.prach_retry_delay_slots == 4, "expected PRACH retry delay");
  mini_gnb_c_require(config.sim.msg3_present, "expected Msg3 burst enabled");
  mini_gnb_c_require(config.sim.include_crnti_ce, "expected simulated C-RNTI CE");
  mini_gnb_c_require(config.broadcast.sib1_period_slots == 8, "expected SIB1 period");
  mini_gnb_c_require(config.broadcast.sib1_offset_slot == 4, "expected SIB1 offset");
}

void test_sib1_schedule_uses_period_and_offset(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  mini_gnb_c_config_t config;
  mini_gnb_c_slot_engine_t slot_engine;
  mini_gnb_c_slot_indication_t slot0;
  mini_gnb_c_slot_indication_t slot4;
  mini_gnb_c_slot_indication_t slot12;
  mini_gnb_c_slot_indication_t slot20;

  mini_gnb_c_default_config_path(config_path, sizeof(config_path));
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) == 0,
                     "expected config to load");

  mini_gnb_c_slot_engine_init(&slot_engine, &config);
  mini_gnb_c_slot_engine_make_slot(&slot_engine, 0, &slot0);
  mini_gnb_c_slot_engine_make_slot(&slot_engine, 4, &slot4);
  mini_gnb_c_slot_engine_make_slot(&slot_engine, 12, &slot12);
  mini_gnb_c_slot_engine_make_slot(&slot_engine, 20, &slot20);

  mini_gnb_c_require(!slot0.has_sib1, "expected no SIB1 at abs_slot 0 when offset is 4");
  mini_gnb_c_require(slot4.has_sib1, "expected SIB1 at abs_slot 4");
  mini_gnb_c_require(slot12.has_sib1, "expected SIB1 at abs_slot 12");
  mini_gnb_c_require(slot20.has_sib1, "expected SIB1 at abs_slot 20");
}
