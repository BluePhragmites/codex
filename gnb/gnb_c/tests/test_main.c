#include <stdio.h>

typedef void (*mini_gnb_c_test_fn)(void);

void test_config_loads(void);
void test_sib1_schedule_uses_period_and_offset(void);
void test_tbsize_lookup_table(void);
void test_ra_manager_flow(void);
void test_ra_timeout(void);
void test_mac_rrc_and_msg4_contention_identity(void);
void test_integration_run(void);
void test_integration_slot_input_prach(void);
void test_integration_slot_text_transport(void);
void test_integration_msg3_missing_retries_prach(void);
void test_integration_msg3_rnti_mismatch_rejected_after_retry(void);
void test_integration_scripted_schedule_files(void);
void test_integration_scripted_pdcch_files(void);

int main(void) {
  struct {
    const char* name;
    mini_gnb_c_test_fn fn;
  } tests[] = {
      {"test_config_loads", test_config_loads},
      {"test_sib1_schedule_uses_period_and_offset", test_sib1_schedule_uses_period_and_offset},
      {"test_tbsize_lookup_table", test_tbsize_lookup_table},
      {"test_ra_manager_flow", test_ra_manager_flow},
      {"test_ra_timeout", test_ra_timeout},
      {"test_mac_rrc_and_msg4_contention_identity", test_mac_rrc_and_msg4_contention_identity},
      {"test_integration_run", test_integration_run},
      {"test_integration_slot_input_prach", test_integration_slot_input_prach},
      {"test_integration_slot_text_transport", test_integration_slot_text_transport},
      {"test_integration_msg3_missing_retries_prach", test_integration_msg3_missing_retries_prach},
      {"test_integration_msg3_rnti_mismatch_rejected_after_retry",
       test_integration_msg3_rnti_mismatch_rejected_after_retry},
      {"test_integration_scripted_schedule_files", test_integration_scripted_schedule_files},
      {"test_integration_scripted_pdcch_files", test_integration_scripted_pdcch_files},
  };
  size_t i = 0;

  for (i = 0; i < (sizeof(tests) / sizeof(tests[0])); ++i) {
    tests[i].fn();
    printf("[PASS] %s\n", tests[i].name);
  }
  return 0;
}
