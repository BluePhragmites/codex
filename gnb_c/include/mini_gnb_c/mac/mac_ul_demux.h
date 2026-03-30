#ifndef MINI_GNB_C_MAC_MAC_UL_DEMUX_H
#define MINI_GNB_C_MAC_MAC_UL_DEMUX_H

#include "mini_gnb_c/common/types.h"

void mini_gnb_c_mac_ul_demux_parse(const mini_gnb_c_buffer_t* mac_pdu,
                                   mini_gnb_c_mac_ul_parse_result_t* out_result);

#endif
