#ifndef MINI_GNB_C_PHY_DL_RAR_BUILDER_H
#define MINI_GNB_C_PHY_DL_RAR_BUILDER_H

#include "mini_gnb_c/common/types.h"

void mini_gnb_c_build_rar_pdu(const mini_gnb_c_ra_schedule_request_t* request,
                              mini_gnb_c_buffer_t* out_rar);

#endif
