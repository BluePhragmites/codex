#pragma once

#include <string>

#include "mini_gnb/broadcast/broadcast_engine.hpp"
#include "mini_gnb/common/types.hpp"
#include "mini_gnb/config/config_loader.hpp"
#include "mini_gnb/mac/mac_ul_demux.hpp"
#include "mini_gnb/metrics/metrics_trace.hpp"
#include "mini_gnb/phy_dl/dl_phy_mapper.hpp"
#include "mini_gnb/phy_ul/prach_detector.hpp"
#include "mini_gnb/phy_ul/pusch_msg3_receiver.hpp"
#include "mini_gnb/ra/ra_manager.hpp"
#include "mini_gnb/radio/radio_frontend.hpp"
#include "mini_gnb/rrc/rrc_ccch_stub.hpp"
#include "mini_gnb/scheduler/initial_access_scheduler.hpp"
#include "mini_gnb/timing/slot_engine.hpp"
#include "mini_gnb/ue/ue_context_store.hpp"

namespace mini_gnb {

class MiniGnbSimulator {
 public:
  MiniGnbSimulator(Config config, std::string output_dir);

  RunSummary run();

 private:
  Config config_;
  MetricsTrace metrics_;
  SlotEngine slot_engine_;
  MockRadioFrontend radio_;
  BroadcastEngine broadcast_;
  MockPrachDetector prach_detector_;
  RaManager ra_manager_;
  InitialAccessScheduler scheduler_;
  MockMsg3Receiver msg3_receiver_;
  MacUlDemux mac_demux_;
  RrcCcchStub rrc_stub_;
  UeContextStore ue_store_;
  MockDlPhyMapper dl_mapper_;
};

}  // namespace mini_gnb
