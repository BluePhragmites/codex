#ifndef MINI_GNB_C_RADIO_HOST_PERFORMANCE_H
#define MINI_GNB_C_RADIO_HOST_PERFORMANCE_H

#include <stddef.h>

#include "mini_gnb_c/common/types.h"

typedef struct {
  int apply_cpu_governor_performance;
  int disable_drm_kms_polling;
  int tune_network_buffers;
} mini_gnb_c_radio_host_performance_plan_t;

void mini_gnb_c_radio_host_performance_plan_for_backend(
    mini_gnb_c_radio_backend_kind_t kind,
    mini_gnb_c_radio_host_performance_plan_t* plan);

int mini_gnb_c_radio_host_performance_prepare_for_backend(
    mini_gnb_c_radio_backend_kind_t kind,
    char* summary,
    size_t summary_size);

#endif
