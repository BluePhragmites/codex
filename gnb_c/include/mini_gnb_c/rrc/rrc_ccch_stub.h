#ifndef MINI_GNB_C_RRC_RRC_CCCH_STUB_H
#define MINI_GNB_C_RRC_RRC_CCCH_STUB_H

#include "mini_gnb_c/common/types.h"

void mini_gnb_c_parse_rrc_setup_request(const mini_gnb_c_buffer_t* ccch_sdu,
                                        mini_gnb_c_rrc_setup_request_info_t* out_request);

void mini_gnb_c_build_rrc_setup(const mini_gnb_c_rrc_setup_request_info_t* request,
                                const mini_gnb_c_sim_config_t* sim,
                                mini_gnb_c_rrc_setup_blob_t* out_setup);

#endif
