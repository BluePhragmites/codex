#include <string.h>

#include "test_helpers.h"

#include "mini_gnb_c/config/config_loader.h"

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
  mini_gnb_c_require(config.sim.include_crnti_ce, "expected simulated C-RNTI CE");
}
