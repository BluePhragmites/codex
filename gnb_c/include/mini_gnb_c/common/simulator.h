#ifndef MINI_GNB_C_COMMON_SIMULATOR_H
#define MINI_GNB_C_COMMON_SIMULATOR_H

#include "mini_gnb_c/broadcast/broadcast_engine.h"
#include "mini_gnb_c/common/types.h"
#include "mini_gnb_c/config/config_loader.h"
#include "mini_gnb_c/core/gnb_core_bridge.h"
#include "mini_gnb_c/mac/mac_ul_demux.h"
#include "mini_gnb_c/metrics/metrics_trace.h"
#include "mini_gnb_c/n3/n3_user_plane.h"
#include "mini_gnb_c/phy_dl/mock_dl_phy_mapper.h"
#include "mini_gnb_c/phy_ul/mock_msg3_receiver.h"
#include "mini_gnb_c/phy_ul/mock_prach_detector.h"
#include "mini_gnb_c/ra/ra_manager.h"
#include "mini_gnb_c/radio/radio_frontend.h"
#include "mini_gnb_c/rrc/rrc_ccch_stub.h"
#include "mini_gnb_c/scheduler/initial_access_scheduler.h"
#include "mini_gnb_c/timing/slot_engine.h"
#include "mini_gnb_c/ue/ue_context_store.h"

typedef struct {
  mini_gnb_c_config_t config;
  mini_gnb_c_metrics_trace_t metrics;
  mini_gnb_c_slot_engine_t slot_engine;
  mini_gnb_c_radio_frontend_t radio;
  mini_gnb_c_broadcast_engine_t broadcast;
  mini_gnb_c_mock_prach_detector_t prach_detector;
  mini_gnb_c_ra_manager_t ra_manager;
  mini_gnb_c_initial_access_scheduler_t scheduler;
  mini_gnb_c_mock_msg3_receiver_t msg3_receiver;
  mini_gnb_c_mock_dl_phy_mapper_t dl_mapper;
  mini_gnb_c_ue_context_store_t ue_store;
  mini_gnb_c_gnb_core_bridge_t core_bridge;
  mini_gnb_c_n3_user_plane_t n3_user_plane;
} mini_gnb_c_simulator_t;

void mini_gnb_c_simulator_init(mini_gnb_c_simulator_t* simulator,
                               const mini_gnb_c_config_t* config,
                               const char* output_dir);

int mini_gnb_c_simulator_run(mini_gnb_c_simulator_t* simulator,
                             mini_gnb_c_run_summary_t* out_summary);

#endif
