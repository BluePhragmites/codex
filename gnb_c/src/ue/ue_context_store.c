#include "mini_gnb_c/ue/ue_context_store.h"

#include <string.h>

void mini_gnb_c_ue_context_store_init(mini_gnb_c_ue_context_store_t* store) {
  if (store == NULL) {
    return;
  }
  memset(store, 0, sizeof(*store));
}

mini_gnb_c_ue_context_t* mini_gnb_c_ue_context_store_promote(
    mini_gnb_c_ue_context_store_t* store,
    const mini_gnb_c_ra_context_t* ra_context,
    const mini_gnb_c_rrc_setup_request_info_t* request_info,
    const int create_abs_slot) {
  mini_gnb_c_ue_context_t context;
  if (store == NULL || ra_context == NULL || request_info == NULL) {
    return NULL;
  }

  memset(&context, 0, sizeof(context));
  context.tc_rnti = ra_context->tc_rnti;
  context.c_rnti = ra_context->tc_rnti;
  memcpy(context.contention_id48, request_info->contention_id48, 6U);
  mini_gnb_c_core_session_reset(&context.core_session);
  mini_gnb_c_core_session_set_c_rnti(&context.core_session, context.c_rnti);
  context.create_abs_slot = create_abs_slot;
  context.rrc_setup_sent = false;
  context.sent_abs_slot = -1;
  context.traffic_plan_scheduled = false;
  context.dl_data_sent = false;
  context.dl_data_abs_slot = -1;
  context.pucch_sr_detected = false;
  context.pucch_sr_abs_slot = -1;
  context.ul_bsr_received = false;
  context.ul_bsr_abs_slot = -1;
  context.ul_bsr_buffer_size_bytes = 0;
  context.connected_ul_pending_bytes = 0;
  context.connected_ul_last_reported_bsr_bytes = 0;
  context.small_ul_grant_abs_slot = -1;
  context.large_ul_grant_abs_slot = -1;
  context.ul_data_received = false;
  context.ul_data_abs_slot = -1;

  store->contexts[0] = context;
  store->count = 1;
  return &store->contexts[0];
}

void mini_gnb_c_ue_context_store_mark_rrc_setup_sent(mini_gnb_c_ue_context_store_t* store,
                                                     const uint16_t tc_rnti,
                                                     const int sent_abs_slot) {
  if (store == NULL) {
    return;
  }
  if (store->count > 0U && store->contexts[0].tc_rnti == tc_rnti) {
    store->contexts[0].rrc_setup_sent = true;
    store->contexts[0].sent_abs_slot = sent_abs_slot;
  }
}
