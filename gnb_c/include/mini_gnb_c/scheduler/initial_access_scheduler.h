#ifndef MINI_GNB_C_SCHEDULER_INITIAL_ACCESS_SCHEDULER_H
#define MINI_GNB_C_SCHEDULER_INITIAL_ACCESS_SCHEDULER_H

#include <stddef.h>

#include "mini_gnb_c/common/types.h"

struct mini_gnb_c_metrics_trace;

typedef struct {
  mini_gnb_c_dl_grant_t pending_dl[MINI_GNB_C_MAX_GRANTS];
  size_t pending_dl_count;
  mini_gnb_c_ul_grant_for_msg3_t pending_ul[MINI_GNB_C_MAX_MSG3_GRANTS];
  size_t pending_ul_count;
  mini_gnb_c_ul_data_grant_t pending_ul_data_pdcch[MINI_GNB_C_MAX_UL_DATA_GRANTS];
  size_t pending_ul_data_pdcch_count;
  mini_gnb_c_ul_data_grant_t pending_ul_data_rx[MINI_GNB_C_MAX_UL_DATA_GRANTS];
  size_t pending_ul_data_rx_count;
} mini_gnb_c_initial_access_scheduler_t;

void mini_gnb_c_initial_access_scheduler_init(mini_gnb_c_initial_access_scheduler_t* scheduler);

void mini_gnb_c_initial_access_scheduler_queue_rar(mini_gnb_c_initial_access_scheduler_t* scheduler,
                                                   const mini_gnb_c_ra_schedule_request_t* request,
                                                   struct mini_gnb_c_metrics_trace* metrics);

void mini_gnb_c_initial_access_scheduler_queue_msg4(mini_gnb_c_initial_access_scheduler_t* scheduler,
                                                    const mini_gnb_c_msg4_schedule_request_t* request,
                                                    struct mini_gnb_c_metrics_trace* metrics);

void mini_gnb_c_initial_access_scheduler_queue_dl_data(mini_gnb_c_initial_access_scheduler_t* scheduler,
                                                       const mini_gnb_c_dl_data_schedule_request_t* request,
                                                       struct mini_gnb_c_metrics_trace* metrics);

void mini_gnb_c_initial_access_scheduler_queue_ul_data_grant(
    mini_gnb_c_initial_access_scheduler_t* scheduler,
    const mini_gnb_c_ul_data_schedule_request_t* request,
    struct mini_gnb_c_metrics_trace* metrics);

size_t mini_gnb_c_initial_access_scheduler_pop_due_downlink(mini_gnb_c_initial_access_scheduler_t* scheduler,
                                                            int abs_slot,
                                                            mini_gnb_c_dl_grant_t* out_grants,
                                                            size_t max_grants);

size_t mini_gnb_c_initial_access_scheduler_pop_due_msg3_grants(
    mini_gnb_c_initial_access_scheduler_t* scheduler,
    int abs_slot,
    mini_gnb_c_ul_grant_for_msg3_t* out_grants,
    size_t max_grants);

size_t mini_gnb_c_initial_access_scheduler_pop_due_ul_data_pdcch(
    mini_gnb_c_initial_access_scheduler_t* scheduler,
    int abs_slot,
    mini_gnb_c_ul_data_grant_t* out_grants,
    size_t max_grants);

size_t mini_gnb_c_initial_access_scheduler_pop_due_ul_data_rx(
    mini_gnb_c_initial_access_scheduler_t* scheduler,
    int abs_slot,
    mini_gnb_c_ul_data_grant_t* out_grants,
    size_t max_grants);

#endif
