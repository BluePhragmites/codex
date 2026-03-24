#include "test_helpers.hpp"

#include "mini_gnb/config/config_loader.hpp"

void test_config_loads() {
  const auto config = mini_gnb::load_config(default_config_path());
  require(config.cell.pci == 1, "expected pci=1");
  require(config.cell.band == 78, "expected band n78");
  require(config.prach.ra_resp_window == 4, "expected ra response window");
  require(config.rf.device_driver == "mock", "expected mock radio frontend");
  require(config.sim.total_slots == 18, "expected total slot count");
  require(config.sim.include_crnti_ce, "expected simulated C-RNTI CE");
}
