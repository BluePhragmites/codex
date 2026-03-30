#ifndef MINI_GNB_C_UE_UE_CONTEXT_STORE_H
#define MINI_GNB_C_UE_UE_CONTEXT_STORE_H

#include <stddef.h>
#include <stdint.h>

#include "mini_gnb_c/common/types.h"

typedef struct {
  mini_gnb_c_ue_context_t contexts[MINI_GNB_C_MAX_UES];
  size_t count;
} mini_gnb_c_ue_context_store_t;

void mini_gnb_c_ue_context_store_init(mini_gnb_c_ue_context_store_t* store);

mini_gnb_c_ue_context_t* mini_gnb_c_ue_context_store_promote(
    mini_gnb_c_ue_context_store_t* store,
    const mini_gnb_c_ra_context_t* ra_context,
    const mini_gnb_c_rrc_setup_request_info_t* request_info,
    int create_abs_slot);

void mini_gnb_c_ue_context_store_mark_rrc_setup_sent(mini_gnb_c_ue_context_store_t* store,
                                                     uint16_t tc_rnti,
                                                     int sent_abs_slot);

#endif
