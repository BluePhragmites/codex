#include <stdio.h>

typedef void (*mini_gnb_c_test_fn)(void);

void test_config_loads(void);
void test_sib1_schedule_uses_period_and_offset(void);
void test_ra_manager_flow(void);
void test_ra_timeout(void);
void test_mac_rrc_and_msg4_contention_identity(void);
void test_integration_run(void);
void test_integration_msg3_missing_retries_prach(void);

int main(void) {
  struct {
    const char* name;
    mini_gnb_c_test_fn fn;
  } tests[] = {
      {"test_config_loads", test_config_loads},
      {"test_sib1_schedule_uses_period_and_offset", test_sib1_schedule_uses_period_and_offset},
      {"test_ra_manager_flow", test_ra_manager_flow},
      {"test_ra_timeout", test_ra_timeout},
      {"test_mac_rrc_and_msg4_contention_identity", test_mac_rrc_and_msg4_contention_identity},
      {"test_integration_run", test_integration_run},
      {"test_integration_msg3_missing_retries_prach", test_integration_msg3_missing_retries_prach},
  };
  size_t i = 0;

  for (i = 0; i < (sizeof(tests) / sizeof(tests[0])); ++i) {
    tests[i].fn();
    printf("[PASS] %s\n", tests[i].name);
  }
  return 0;
}
