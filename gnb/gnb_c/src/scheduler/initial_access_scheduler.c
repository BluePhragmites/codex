#include "mini_gnb_c/scheduler/initial_access_scheduler.h"

#include <stdio.h>
#include <string.h>

#include "mini_gnb_c/common/hex.h"
#include "mini_gnb_c/metrics/metrics_trace.h"
#include "mini_gnb_c/phy_dl/msg4_builder.h"
#include "mini_gnb_c/phy_dl/rar_builder.h"

void mini_gnb_c_initial_access_scheduler_init(mini_gnb_c_initial_access_scheduler_t* scheduler) {
  if (scheduler == NULL) {
    return;
  }
  memset(scheduler, 0, sizeof(*scheduler));
}

void mini_gnb_c_initial_access_scheduler_queue_rar(mini_gnb_c_initial_access_scheduler_t* scheduler,
                                                   const mini_gnb_c_ra_schedule_request_t* request,
                                                   mini_gnb_c_metrics_trace_t* metrics) {
  char hex[MINI_GNB_C_MAX_TEXT];
  mini_gnb_c_buffer_t rar_pdu;
  mini_gnb_c_dl_grant_t* grant = NULL;

  if (scheduler == NULL || request == NULL || metrics == NULL ||
      scheduler->pending_dl_count >= MINI_GNB_C_MAX_GRANTS ||
      scheduler->pending_ul_count >= MINI_GNB_C_MAX_MSG3_GRANTS) {
    return;
  }

  mini_gnb_c_build_rar_pdu(request, &rar_pdu);
  grant = &scheduler->pending_dl[scheduler->pending_dl_count++];
  memset(grant, 0, sizeof(*grant));
  grant->type = MINI_GNB_C_DL_OBJ_RAR;
  grant->abs_slot = request->rar_abs_slot;
  grant->rnti = request->tc_rnti;
  grant->prb_start = 44;
  grant->prb_len = 12;
  grant->mcs = 4;
  grant->payload = rar_pdu;

  scheduler->pending_ul[scheduler->pending_ul_count++] = request->ul_grant;
  (void)mini_gnb_c_bytes_to_hex(rar_pdu.bytes, rar_pdu.len, hex, sizeof(hex));
  mini_gnb_c_metrics_trace_event(metrics,
                                 "initial_access_scheduler",
                                 "Queued RAR and Msg3 grant.",
                                 request->rar_abs_slot,
                                 "tc_rnti=%u,msg3_abs_slot=%d,rar_payload=%s",
                                 request->tc_rnti,
                                 request->ul_grant.abs_slot,
                                 hex);
}

void mini_gnb_c_initial_access_scheduler_queue_msg4(mini_gnb_c_initial_access_scheduler_t* scheduler,
                                                    const mini_gnb_c_msg4_schedule_request_t* request,
                                                    mini_gnb_c_metrics_trace_t* metrics) {
  char hex[MINI_GNB_C_MAX_TEXT];
  mini_gnb_c_buffer_t msg4_pdu;
  mini_gnb_c_dl_grant_t* grant = NULL;

  if (scheduler == NULL || request == NULL || metrics == NULL ||
      scheduler->pending_dl_count >= MINI_GNB_C_MAX_GRANTS) {
    return;
  }

  mini_gnb_c_build_msg4_pdu(request, &msg4_pdu);
  grant = &scheduler->pending_dl[scheduler->pending_dl_count++];
  memset(grant, 0, sizeof(*grant));
  grant->type = MINI_GNB_C_DL_OBJ_MSG4;
  grant->abs_slot = request->msg4_abs_slot;
  grant->rnti = request->tc_rnti;
  grant->prb_start = 48;
  grant->prb_len = 16;
  grant->mcs = 4;
  grant->payload = msg4_pdu;

  (void)mini_gnb_c_bytes_to_hex(msg4_pdu.bytes, msg4_pdu.len, hex, sizeof(hex));
  mini_gnb_c_metrics_trace_event(metrics,
                                 "initial_access_scheduler",
                                 "Queued Msg4 transmission.",
                                 request->msg4_abs_slot,
                                 "tc_rnti=%u,msg4_payload=%s",
                                 request->tc_rnti,
                                 hex);
}

static size_t mini_gnb_c_pop_due_dl(mini_gnb_c_dl_grant_t* pending,
                                    size_t* pending_count,
                                    const int abs_slot,
                                    mini_gnb_c_dl_grant_t* out_grants,
                                    const size_t max_grants) {
  size_t i = 0;
  size_t out_count = 0;
  size_t write_index = 0;

  for (i = 0; i < *pending_count; ++i) {
    if (pending[i].abs_slot == abs_slot && out_count < max_grants) {
      out_grants[out_count++] = pending[i];
    } else {
      pending[write_index++] = pending[i];
    }
  }
  *pending_count = write_index;
  return out_count;
}

static size_t mini_gnb_c_pop_due_ul(mini_gnb_c_ul_grant_for_msg3_t* pending,
                                    size_t* pending_count,
                                    const int abs_slot,
                                    mini_gnb_c_ul_grant_for_msg3_t* out_grants,
                                    const size_t max_grants) {
  size_t i = 0;
  size_t out_count = 0;
  size_t write_index = 0;

  for (i = 0; i < *pending_count; ++i) {
    if (pending[i].abs_slot == abs_slot && out_count < max_grants) {
      out_grants[out_count++] = pending[i];
    } else {
      pending[write_index++] = pending[i];
    }
  }
  *pending_count = write_index;
  return out_count;
}

size_t mini_gnb_c_initial_access_scheduler_pop_due_downlink(mini_gnb_c_initial_access_scheduler_t* scheduler,
                                                            const int abs_slot,
                                                            mini_gnb_c_dl_grant_t* out_grants,
                                                            const size_t max_grants) {
  if (scheduler == NULL || out_grants == NULL) {
    return 0;
  }
  return mini_gnb_c_pop_due_dl(scheduler->pending_dl,
                               &scheduler->pending_dl_count,
                               abs_slot,
                               out_grants,
                               max_grants);
}

size_t mini_gnb_c_initial_access_scheduler_pop_due_msg3_grants(
    mini_gnb_c_initial_access_scheduler_t* scheduler,
    const int abs_slot,
    mini_gnb_c_ul_grant_for_msg3_t* out_grants,
    const size_t max_grants) {
  if (scheduler == NULL || out_grants == NULL) {
    return 0;
  }
  return mini_gnb_c_pop_due_ul(scheduler->pending_ul,
                               &scheduler->pending_ul_count,
                               abs_slot,
                               out_grants,
                               max_grants);
}
