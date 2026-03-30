#ifndef MINI_GNB_C_RA_RA_MANAGER_H
#define MINI_GNB_C_RA_RA_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "mini_gnb_c/common/types.h"

struct mini_gnb_c_metrics_trace;

typedef struct {
  mini_gnb_c_prach_config_t prach;
  mini_gnb_c_sim_config_t sim;
  bool has_active_context;
  mini_gnb_c_ra_context_t active_context;
  uint16_t next_tc_rnti;
} mini_gnb_c_ra_manager_t;

void mini_gnb_c_ra_manager_init(mini_gnb_c_ra_manager_t* manager,
                                const mini_gnb_c_prach_config_t* prach,
                                const mini_gnb_c_sim_config_t* sim);

bool mini_gnb_c_ra_manager_on_prach(mini_gnb_c_ra_manager_t* manager,
                                    const mini_gnb_c_prach_indication_t* prach_indication,
                                    const mini_gnb_c_slot_indication_t* slot,
                                    struct mini_gnb_c_metrics_trace* metrics,
                                    mini_gnb_c_ra_schedule_request_t* out_request);

void mini_gnb_c_ra_manager_mark_rar_sent(mini_gnb_c_ra_manager_t* manager,
                                         uint16_t tc_rnti,
                                         const mini_gnb_c_slot_indication_t* slot,
                                         struct mini_gnb_c_metrics_trace* metrics);

bool mini_gnb_c_ra_manager_on_msg3_success(mini_gnb_c_ra_manager_t* manager,
                                           const mini_gnb_c_msg3_decode_indication_t* msg3,
                                           const mini_gnb_c_mac_ul_parse_result_t* mac_result,
                                           const mini_gnb_c_rrc_setup_request_info_t* request_info,
                                           const mini_gnb_c_rrc_setup_blob_t* rrc_setup,
                                           const mini_gnb_c_slot_indication_t* slot,
                                           struct mini_gnb_c_metrics_trace* metrics,
                                           mini_gnb_c_msg4_schedule_request_t* out_request);

void mini_gnb_c_ra_manager_mark_msg4_sent(mini_gnb_c_ra_manager_t* manager,
                                          uint16_t tc_rnti,
                                          const mini_gnb_c_slot_indication_t* slot,
                                          struct mini_gnb_c_metrics_trace* metrics);

void mini_gnb_c_ra_manager_expire(mini_gnb_c_ra_manager_t* manager,
                                  const mini_gnb_c_slot_indication_t* slot,
                                  struct mini_gnb_c_metrics_trace* metrics);

#endif
