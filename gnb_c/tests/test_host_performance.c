#include <string.h>

#include "test_helpers.h"

#include "mini_gnb_c/radio/host_performance.h"

void test_host_performance_plan_for_b210_skips_network_buffers(void) {
  mini_gnb_c_radio_host_performance_plan_t plan;

  memset(&plan, 0, sizeof(plan));
  mini_gnb_c_radio_host_performance_plan_for_backend(MINI_GNB_C_RADIO_BACKEND_B210, &plan);

  mini_gnb_c_require(plan.apply_cpu_governor_performance == 1, "expected CPU governor tuning for B210");
  mini_gnb_c_require(plan.disable_drm_kms_polling == 1, "expected KMS polling tuning for B210");
  mini_gnb_c_require(plan.tune_network_buffers == 0, "expected network buffers skipped for USB B210");
}

void test_host_performance_plan_for_mock_is_not_applicable(void) {
  mini_gnb_c_radio_host_performance_plan_t plan;
  char summary[128];

  memset(&plan, 0, sizeof(plan));
  mini_gnb_c_radio_host_performance_plan_for_backend(MINI_GNB_C_RADIO_BACKEND_MOCK, &plan);

  mini_gnb_c_require(plan.apply_cpu_governor_performance == 0, "expected no CPU tuning for mock");
  mini_gnb_c_require(plan.disable_drm_kms_polling == 0, "expected no KMS tuning for mock");
  mini_gnb_c_require(plan.tune_network_buffers == 0, "expected no network tuning for mock");

  memset(summary, 0, sizeof(summary));
  mini_gnb_c_require(
      mini_gnb_c_radio_host_performance_prepare_for_backend(MINI_GNB_C_RADIO_BACKEND_MOCK, summary, sizeof(summary)) ==
          0,
      "expected mock host tuning summary");
  mini_gnb_c_require(strcmp(summary, "host_tuning=not_applicable") == 0,
                     "expected mock backend to skip host tuning");
}
