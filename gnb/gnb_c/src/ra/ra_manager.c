#include "mini_gnb_c/ra/ra_manager.h"

#include <stdio.h>
#include <string.h>

#include "mini_gnb_c/common/hex.h"
#include "mini_gnb_c/metrics/metrics_trace.h"

void mini_gnb_c_ra_manager_init(mini_gnb_c_ra_manager_t* manager,
                                const mini_gnb_c_prach_config_t* prach,
                                const mini_gnb_c_sim_config_t* sim) {
  if (manager == NULL || prach == NULL || sim == NULL) {
    return;
  }

  memset(manager, 0, sizeof(*manager));
  memcpy(&manager->prach, prach, sizeof(*prach));
  memcpy(&manager->sim, sim, sizeof(*sim));
  manager->next_tc_rnti = 0x4601U;
}

bool mini_gnb_c_ra_manager_on_prach(mini_gnb_c_ra_manager_t* manager,
                                    const mini_gnb_c_prach_indication_t* prach_indication,
                                    const mini_gnb_c_slot_indication_t* slot,
                                    mini_gnb_c_metrics_trace_t* metrics,
                                    mini_gnb_c_ra_schedule_request_t* out_request) {
  mini_gnb_c_ra_context_t context;

  if (manager == NULL || prach_indication == NULL || slot == NULL || metrics == NULL ||
      out_request == NULL) {
    return false;
  }

  if (manager->has_active_context && manager->active_context.state != MINI_GNB_C_RA_DONE &&
      manager->active_context.state != MINI_GNB_C_RA_FAIL) {
    mini_gnb_c_metrics_trace_event(metrics,
                                   "ra_manager",
                                   "Ignored PRACH because a RA context is already active.",
                                   slot->abs_slot,
                                   "active_tc_rnti=%u",
                                   manager->active_context.tc_rnti);
    return false;
  }

  memset(&context, 0, sizeof(context));
  context.detect_abs_slot = slot->abs_slot;
  context.preamble_id = prach_indication->preamble_id;
  context.tc_rnti = manager->next_tc_rnti++;
  context.ta_est = prach_indication->ta_est;
  context.rar_abs_slot = slot->abs_slot + 1;
  context.msg3_expect_abs_slot = slot->abs_slot + manager->sim.msg3_delay_slots;
  context.msg4_abs_slot = -1;
  context.state = MINI_GNB_C_RA_TC_RNTI_ASSIGNED;
  context.last_failure[0] = '\0';

  manager->active_context = context;
  manager->has_active_context = true;

  mini_gnb_c_metrics_trace_event(metrics,
                                 "ra_manager",
                                 "Created RA context from PRACH.",
                                 slot->abs_slot,
                                 "preamble_id=%u,tc_rnti=%u,rar_abs_slot=%d,msg3_expect_abs_slot=%d",
                                 context.preamble_id,
                                 context.tc_rnti,
                                 context.rar_abs_slot,
                                 context.msg3_expect_abs_slot);

  memset(out_request, 0, sizeof(*out_request));
  out_request->tc_rnti = context.tc_rnti;
  out_request->detect_abs_slot = slot->abs_slot;
  out_request->rar_abs_slot = context.rar_abs_slot;
  out_request->preamble_id = prach_indication->preamble_id;
  out_request->ta_cmd = (uint8_t)prach_indication->ta_est;
  out_request->ul_grant.tc_rnti = context.tc_rnti;
  out_request->ul_grant.abs_slot = context.msg3_expect_abs_slot;
  out_request->ul_grant.msg3_prb_start = 48U;
  out_request->ul_grant.msg3_prb_len = 16U;
  out_request->ul_grant.msg3_mcs = 4U;
  out_request->ul_grant.k2 = 4U;
  out_request->ul_grant.ta_cmd = (uint8_t)prach_indication->ta_est;
  return true;
}

void mini_gnb_c_ra_manager_mark_rar_sent(mini_gnb_c_ra_manager_t* manager,
                                         const uint16_t tc_rnti,
                                         const mini_gnb_c_slot_indication_t* slot,
                                         mini_gnb_c_metrics_trace_t* metrics) {
  if (manager == NULL || slot == NULL || metrics == NULL || !manager->has_active_context ||
      manager->active_context.tc_rnti != tc_rnti) {
    return;
  }

  manager->active_context.state = MINI_GNB_C_RA_MSG3_WAIT;
  mini_gnb_c_metrics_trace_event(metrics,
                                 "ra_manager",
                                 "RAR sent, waiting for Msg3.",
                                 slot->abs_slot,
                                 "tc_rnti=%u,msg3_expect_abs_slot=%d",
                                 tc_rnti,
                                 manager->active_context.msg3_expect_abs_slot);
}

bool mini_gnb_c_ra_manager_on_msg3_success(mini_gnb_c_ra_manager_t* manager,
                                           const mini_gnb_c_msg3_decode_indication_t* msg3,
                                           const mini_gnb_c_mac_ul_parse_result_t* mac_result,
                                           const mini_gnb_c_rrc_setup_request_info_t* request_info,
                                           const mini_gnb_c_rrc_setup_blob_t* rrc_setup,
                                           const mini_gnb_c_slot_indication_t* slot,
                                           mini_gnb_c_metrics_trace_t* metrics,
                                           mini_gnb_c_msg4_schedule_request_t* out_request) {
  char contention_id_hex[MINI_GNB_C_MAX_TEXT];
  (void)mac_result;

  if (manager == NULL || msg3 == NULL || request_info == NULL || rrc_setup == NULL || slot == NULL ||
      metrics == NULL || out_request == NULL || !manager->has_active_context ||
      manager->active_context.tc_rnti != msg3->rnti) {
    return false;
  }

  manager->active_context.state = MINI_GNB_C_RA_MSG3_OK;
  manager->active_context.msg4_abs_slot = slot->abs_slot + manager->sim.msg4_delay_slots;
  memcpy(manager->active_context.contention_id48, request_info->contention_id48, 6U);
  manager->active_context.has_contention_id = true;
  manager->active_context.ue_ctx_promoted = true;

  (void)mini_gnb_c_bytes_to_hex(manager->active_context.contention_id48,
                                6U,
                                contention_id_hex,
                                sizeof(contention_id_hex));
  mini_gnb_c_metrics_trace_event(metrics,
                                 "ra_manager",
                                 "Msg3 accepted, scheduling Msg4.",
                                 slot->abs_slot,
                                 "tc_rnti=%u,contention_id48=%s,msg4_abs_slot=%d",
                                 manager->active_context.tc_rnti,
                                 contention_id_hex,
                                 manager->active_context.msg4_abs_slot);

  memset(out_request, 0, sizeof(*out_request));
  out_request->tc_rnti = manager->active_context.tc_rnti;
  out_request->msg4_abs_slot = manager->active_context.msg4_abs_slot;
  memcpy(out_request->contention_id48, request_info->contention_id48, 6U);
  memcpy(&out_request->rrc_setup, rrc_setup, sizeof(*rrc_setup));
  return true;
}

void mini_gnb_c_ra_manager_mark_msg4_sent(mini_gnb_c_ra_manager_t* manager,
                                          const uint16_t tc_rnti,
                                          const mini_gnb_c_slot_indication_t* slot,
                                          mini_gnb_c_metrics_trace_t* metrics) {
  if (manager == NULL || slot == NULL || metrics == NULL || !manager->has_active_context ||
      manager->active_context.tc_rnti != tc_rnti) {
    return;
  }

  manager->active_context.state = MINI_GNB_C_RA_DONE;
  mini_gnb_c_metrics_trace_event(metrics,
                                 "ra_manager",
                                 "Msg4 sent, RA flow completed.",
                                 slot->abs_slot,
                                 "tc_rnti=%u",
                                 tc_rnti);
}

void mini_gnb_c_ra_manager_expire(mini_gnb_c_ra_manager_t* manager,
                                  const mini_gnb_c_slot_indication_t* slot,
                                  mini_gnb_c_metrics_trace_t* metrics) {
  const char* timeout_reason = NULL;

  if (manager == NULL || slot == NULL || metrics == NULL || !manager->has_active_context) {
    return;
  }

  if (manager->active_context.state == MINI_GNB_C_RA_TC_RNTI_ASSIGNED &&
      slot->abs_slot > (manager->active_context.rar_abs_slot + (int)manager->prach.ra_resp_window)) {
    timeout_reason = "RAR_TIMEOUT";
  } else if (manager->active_context.state == MINI_GNB_C_RA_MSG3_WAIT &&
             slot->abs_slot > (manager->active_context.msg3_expect_abs_slot + 2)) {
    timeout_reason = "MSG3_TIMEOUT";
  }

  if (timeout_reason == NULL) {
    return;
  }

  manager->active_context.state = MINI_GNB_C_RA_FAIL;
  (void)snprintf(manager->active_context.last_failure,
                 sizeof(manager->active_context.last_failure),
                 "%s",
                 timeout_reason);
  mini_gnb_c_metrics_trace_increment_named(metrics, "ra_timeout", 1U);
  mini_gnb_c_metrics_trace_event(metrics,
                                 "ra_manager",
                                 "RA context expired.",
                                 slot->abs_slot,
                                 "tc_rnti=%u,reason=%s",
                                 manager->active_context.tc_rnti,
                                 timeout_reason);
}
