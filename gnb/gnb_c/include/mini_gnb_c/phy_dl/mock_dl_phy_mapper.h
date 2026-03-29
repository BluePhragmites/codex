#ifndef MINI_GNB_C_PHY_DL_MOCK_DL_PHY_MAPPER_H
#define MINI_GNB_C_PHY_DL_MOCK_DL_PHY_MAPPER_H

#include <stddef.h>

#include "mini_gnb_c/common/types.h"

typedef struct {
  int unused;
} mini_gnb_c_mock_dl_phy_mapper_t;

void mini_gnb_c_mock_dl_phy_mapper_init(mini_gnb_c_mock_dl_phy_mapper_t* mapper);

size_t mini_gnb_c_mock_dl_phy_mapper_map(const mini_gnb_c_mock_dl_phy_mapper_t* mapper,
                                         const mini_gnb_c_slot_indication_t* slot,
                                         const mini_gnb_c_dl_grant_t* grants,
                                         size_t grant_count,
                                         mini_gnb_c_tx_grid_patch_t* out_patches,
                                         size_t max_patches);

#endif
