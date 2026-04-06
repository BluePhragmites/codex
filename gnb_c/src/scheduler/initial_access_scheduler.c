#include "mini_gnb_c/scheduler/initial_access_scheduler.h"

#include <stdio.h>
#include <string.h>

#include "mini_gnb_c/common/hex.h"
#include "mini_gnb_c/metrics/metrics_trace.h"
#include "mini_gnb_c/phy_dl/msg4_builder.h"
#include "mini_gnb_c/phy_dl/rar_builder.h"

static void mini_gnb_c_assign_pdcch(mini_gnb_c_pdcch_dci_t* pdcch,
                                    const mini_gnb_c_dci_format_t format,
                                    const uint16_t rnti,
                                    const uint16_t coreset_prb_start,
                                    const uint16_t coreset_prb_len,
                                    const uint16_t scheduled_prb_start,
                                    const uint16_t scheduled_prb_len,
                                    const uint8_t mcs,
                                    const int k2,
                                    const int time_indicator,
                                    const int dl_data_to_ul_ack,
                                    const int scheduled_abs_slot,
                                    const mini_gnb_c_dl_object_type_t scheduled_dl_type,
                                    const mini_gnb_c_ul_burst_type_t scheduled_ul_type,
                                    const mini_gnb_c_ul_data_purpose_t scheduled_ul_purpose,
                                    const uint8_t harq_id,
                                    const bool ndi,
                                    const bool is_new_data) {
  if (pdcch == NULL) {
    return;
  }

  memset(pdcch, 0, sizeof(*pdcch));
  pdcch->valid = true;
  pdcch->format = format;
  pdcch->rnti = rnti;
  pdcch->coreset_prb_start = coreset_prb_start;
  pdcch->coreset_prb_len = coreset_prb_len;
  pdcch->scheduled_prb_start = scheduled_prb_start;
  pdcch->scheduled_prb_len = scheduled_prb_len;
  pdcch->mcs = mcs;
  pdcch->k2 = k2;
  pdcch->time_indicator = time_indicator;
  pdcch->dl_data_to_ul_ack = dl_data_to_ul_ack;
  pdcch->scheduled_abs_slot = scheduled_abs_slot;
  pdcch->scheduled_dl_type = scheduled_dl_type;
  pdcch->scheduled_ul_type = scheduled_ul_type;
  pdcch->scheduled_ul_purpose = scheduled_ul_purpose;
  pdcch->harq_id = harq_id;
  pdcch->ndi = ndi;
  pdcch->is_new_data = is_new_data;
}

static void mini_gnb_c_assign_dl_pdcch(mini_gnb_c_dl_grant_t* grant,
                                       const mini_gnb_c_dci_format_t format,
                                       const int time_indicator,
                                       const int dl_data_to_ul_ack,
                                       const uint8_t harq_id,
                                       const bool ndi,
                                       const bool is_new_data) {
  if (grant == NULL) {
    return;
  }

  mini_gnb_c_assign_pdcch(&grant->pdcch,
                          format,
                          grant->rnti,
                          0U,
                          12U,
                          grant->prb_start,
                          grant->prb_len,
                          grant->mcs,
                          -1,
                          time_indicator,
                          dl_data_to_ul_ack,
                          grant->abs_slot,
                          grant->type,
                          MINI_GNB_C_UL_BURST_NONE,
                          MINI_GNB_C_UL_DATA_PURPOSE_PAYLOAD,
                          harq_id,
                          ndi,
                          is_new_data);
}

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
  mini_gnb_c_assign_dl_pdcch(grant, MINI_GNB_C_DCI_FORMAT_1_0, 0, 0, 0u, true, true);
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
  mini_gnb_c_assign_dl_pdcch(grant, MINI_GNB_C_DCI_FORMAT_1_0, 0, 0, 0u, true, true);
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

void mini_gnb_c_initial_access_scheduler_queue_dl_data(mini_gnb_c_initial_access_scheduler_t* scheduler,
                                                       const mini_gnb_c_dl_data_schedule_request_t* request,
                                                       mini_gnb_c_metrics_trace_t* metrics) {
  mini_gnb_c_dl_grant_t* grant = NULL;
  uint16_t tbsize = 0U;

  if (scheduler == NULL || request == NULL || metrics == NULL ||
      scheduler->pending_dl_count >= MINI_GNB_C_MAX_GRANTS ||
      scheduler->pending_dl_pdcch_count >= MINI_GNB_C_MAX_GRANTS) {
    return;
  }

  grant = &scheduler->pending_dl[scheduler->pending_dl_count++];
  memset(grant, 0, sizeof(*grant));
  grant->type = MINI_GNB_C_DL_OBJ_DATA;
  grant->abs_slot = request->abs_slot;
  grant->rnti = request->c_rnti;
  grant->prb_start = request->prb_start;
  grant->prb_len = request->prb_len;
  grant->mcs = request->mcs;
  grant->harq_id = request->harq_id;
  grant->payload = request->payload;
  mini_gnb_c_assign_dl_pdcch(grant,
                             request->dci_format,
                             request->time_indicator,
                             request->dl_data_to_ul_ack,
                             request->harq_id,
                             request->ndi,
                             request->is_new_data);
  if (request->pdcch_abs_slot >= 0) {
    grant->pdcch.scheduled_abs_slot = request->abs_slot;
    scheduler->pending_dl_pdcch[scheduler->pending_dl_pdcch_count++] = grant->pdcch;
  }
  tbsize = mini_gnb_c_lookup_tbsize(grant->prb_len, grant->mcs);

  mini_gnb_c_metrics_trace_event(metrics,
                                 "initial_access_scheduler",
                                 "Queued connected-mode DL data transmission.",
                                 request->pdcch_abs_slot >= 0 ? request->pdcch_abs_slot : request->abs_slot,
                                 "c_rnti=%u,dci=%s,pdcch_abs_slot=%d,dl_abs_slot=%d,tdi=%d,dl_ack=%d,harq_id=%u,ndi=%s,is_new_data=%s,prb_start=%u,prb_len=%u,tbsize=%u,payload_len=%zu",
                                 request->c_rnti,
                                 mini_gnb_c_dci_format_to_string(grant->pdcch.format),
                                 request->pdcch_abs_slot,
                                 request->abs_slot,
                                 request->time_indicator,
                                 request->dl_data_to_ul_ack,
                                 request->harq_id,
                                 request->ndi ? "true" : "false",
                                 request->is_new_data ? "true" : "false",
                                 grant->prb_start,
                                 grant->prb_len,
                                 tbsize,
                                 grant->payload.len);
}

void mini_gnb_c_initial_access_scheduler_queue_ul_data_grant(
    mini_gnb_c_initial_access_scheduler_t* scheduler,
    const mini_gnb_c_ul_data_schedule_request_t* request,
    mini_gnb_c_metrics_trace_t* metrics) {
  mini_gnb_c_ul_data_grant_t* control_grant = NULL;
  mini_gnb_c_ul_data_grant_t* rx_expectation = NULL;
  uint16_t tbsize = 0U;

  if (scheduler == NULL || request == NULL || metrics == NULL ||
      scheduler->pending_ul_data_pdcch_count >= MINI_GNB_C_MAX_UL_DATA_GRANTS ||
      scheduler->pending_ul_data_rx_count >= MINI_GNB_C_MAX_UL_DATA_GRANTS) {
    return;
  }

  control_grant = &scheduler->pending_ul_data_pdcch[scheduler->pending_ul_data_pdcch_count++];
  memset(control_grant, 0, sizeof(*control_grant));
  control_grant->c_rnti = request->c_rnti;
  control_grant->pdcch_abs_slot = request->pdcch_abs_slot;
  control_grant->abs_slot = request->abs_slot;
  control_grant->prb_start = request->prb_start;
  control_grant->prb_len = request->prb_len;
  control_grant->mcs = request->mcs;
  control_grant->k2 = request->k2;
  control_grant->purpose = request->purpose;
  control_grant->harq_id = request->harq_id;
  control_grant->ndi = request->ndi;
  control_grant->is_new_data = request->is_new_data;
  mini_gnb_c_assign_pdcch(&control_grant->pdcch,
                          MINI_GNB_C_DCI_FORMAT_0_1,
                          request->c_rnti,
                          0U,
                          12U,
                          request->prb_start,
                          request->prb_len,
                          request->mcs,
                          request->k2,
                          request->k2,
                          0,
                          request->abs_slot,
                          MINI_GNB_C_DL_OBJ_PDCCH,
                          MINI_GNB_C_UL_BURST_DATA,
                          request->purpose,
                          request->harq_id,
                          request->ndi,
                          request->is_new_data);

  rx_expectation = &scheduler->pending_ul_data_rx[scheduler->pending_ul_data_rx_count++];
  *rx_expectation = *control_grant;
  tbsize = mini_gnb_c_lookup_tbsize(request->prb_len, request->mcs);

  mini_gnb_c_metrics_trace_event(metrics,
                                 "initial_access_scheduler",
                                 "Queued connected-mode UL data grant.",
                                 request->pdcch_abs_slot,
                                 "c_rnti=%u,dci=%s,purpose=%s,ul_abs_slot=%d,prb_start=%u,prb_len=%u,mcs=%u,tbsize=%u,time_indicator=%u,harq_id=%u,ndi=%s,is_new_data=%s",
                                 request->c_rnti,
                                 mini_gnb_c_dci_format_to_string(control_grant->pdcch.format),
                                 request->purpose == MINI_GNB_C_UL_DATA_PURPOSE_BSR ? "BSR" : "PAYLOAD",
                                 request->abs_slot,
                                 request->prb_start,
                                 request->prb_len,
                                 request->mcs,
                                 tbsize,
                                 request->k2,
                                 request->harq_id,
                                 request->ndi ? "true" : "false",
                                 request->is_new_data ? "true" : "false");
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

static size_t mini_gnb_c_pop_due_ul_data(mini_gnb_c_ul_data_grant_t* pending,
                                         size_t* pending_count,
                                         const int abs_slot,
                                         const bool match_pdcch_slot,
                                         mini_gnb_c_ul_data_grant_t* out_grants,
                                         const size_t max_grants) {
  size_t i = 0;
  size_t out_count = 0;
  size_t write_index = 0;

  for (i = 0; i < *pending_count; ++i) {
    const int due_slot = match_pdcch_slot ? pending[i].pdcch_abs_slot : pending[i].abs_slot;
    if (due_slot == abs_slot && out_count < max_grants) {
      out_grants[out_count++] = pending[i];
    } else {
      pending[write_index++] = pending[i];
    }
  }

  *pending_count = write_index;
  return out_count;
}

static size_t mini_gnb_c_pop_due_pdcch(mini_gnb_c_pdcch_dci_t* pending,
                                       size_t* pending_count,
                                       const int abs_slot,
                                       mini_gnb_c_pdcch_dci_t* out_pdcch,
                                       const size_t max_pdcch) {
  size_t i = 0;
  size_t out_count = 0;
  size_t write_index = 0;

  for (i = 0; i < *pending_count; ++i) {
    const int pdcch_abs_slot =
        pending[i].scheduled_abs_slot - pending[i].time_indicator >= 0 ? pending[i].scheduled_abs_slot - pending[i].time_indicator
                                                                        : pending[i].scheduled_abs_slot;
    if (pdcch_abs_slot == abs_slot && out_count < max_pdcch) {
      out_pdcch[out_count++] = pending[i];
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

size_t mini_gnb_c_initial_access_scheduler_pop_due_dl_pdcch(mini_gnb_c_initial_access_scheduler_t* scheduler,
                                                            const int abs_slot,
                                                            mini_gnb_c_pdcch_dci_t* out_pdcch,
                                                            const size_t max_pdcch) {
  if (scheduler == NULL || out_pdcch == NULL) {
    return 0;
  }
  return mini_gnb_c_pop_due_pdcch(scheduler->pending_dl_pdcch,
                                  &scheduler->pending_dl_pdcch_count,
                                  abs_slot,
                                  out_pdcch,
                                  max_pdcch);
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

size_t mini_gnb_c_initial_access_scheduler_pop_due_ul_data_pdcch(
    mini_gnb_c_initial_access_scheduler_t* scheduler,
    const int abs_slot,
    mini_gnb_c_ul_data_grant_t* out_grants,
    const size_t max_grants) {
  if (scheduler == NULL || out_grants == NULL) {
    return 0;
  }

  return mini_gnb_c_pop_due_ul_data(scheduler->pending_ul_data_pdcch,
                                    &scheduler->pending_ul_data_pdcch_count,
                                    abs_slot,
                                    true,
                                    out_grants,
                                    max_grants);
}

size_t mini_gnb_c_initial_access_scheduler_pop_due_ul_data_rx(
    mini_gnb_c_initial_access_scheduler_t* scheduler,
    const int abs_slot,
    mini_gnb_c_ul_data_grant_t* out_grants,
    const size_t max_grants) {
  if (scheduler == NULL || out_grants == NULL) {
    return 0;
  }

  return mini_gnb_c_pop_due_ul_data(scheduler->pending_ul_data_rx,
                                    &scheduler->pending_ul_data_rx_count,
                                    abs_slot,
                                    false,
                                    out_grants,
                                    max_grants);
}
