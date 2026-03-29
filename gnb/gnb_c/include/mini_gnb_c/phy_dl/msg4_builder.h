#ifndef MINI_GNB_C_PHY_DL_MSG4_BUILDER_H
#define MINI_GNB_C_PHY_DL_MSG4_BUILDER_H

#include "mini_gnb_c/common/types.h"

void mini_gnb_c_build_msg4_pdu(const mini_gnb_c_msg4_schedule_request_t* request,
                               mini_gnb_c_buffer_t* out_msg4);

#endif
