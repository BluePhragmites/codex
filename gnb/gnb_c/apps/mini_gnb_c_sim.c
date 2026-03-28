#include <stdio.h>

#include "mini_gnb_c/common/simulator.h"

int main(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char output_dir[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  char config_summary[512];
  mini_gnb_c_config_t config;
  mini_gnb_c_simulator_t simulator;
  mini_gnb_c_run_summary_t summary;

  if (snprintf(config_path,
               sizeof(config_path),
               "%s/config/default_cell.yml",
               MINI_GNB_C_SOURCE_DIR) >= (int)sizeof(config_path)) {
    fprintf(stderr, "failed to construct config path\n");
    return 1;
  }

  if (mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) != 0) {
    fprintf(stderr, "failed to load config: %s\n", error_message);
    return 1;
  }

  if (mini_gnb_c_format_config_summary(&config, config_summary, sizeof(config_summary)) != 0) {
    fprintf(stderr, "failed to format config summary\n");
    return 1;
  }

  printf("%s\n", config_summary);
  if (snprintf(output_dir, sizeof(output_dir), "%s/out", MINI_GNB_C_SOURCE_DIR) >= (int)sizeof(output_dir)) {
    fprintf(stderr, "failed to construct output directory\n");
    return 1;
  }

  mini_gnb_c_simulator_init(&simulator, &config, output_dir);
  if (mini_gnb_c_simulator_run(&simulator, &summary) != 0) {
    fprintf(stderr, "simulation failed\n");
    return 1;
  }

  printf("Simulation finished.\n");
  printf("  PRACH detected: %llu\n", (unsigned long long)summary.counters.prach_detect_ok);
  printf("  RAR sent: %llu\n", (unsigned long long)summary.counters.rar_sent);
  printf("  Msg3 CRC OK: %llu\n", (unsigned long long)summary.counters.msg3_crc_ok);
  printf("  RRCSetup sent: %llu\n", (unsigned long long)summary.counters.rrcsetup_sent);
  printf("Artifacts:\n");
  printf("  trace: %s\n", summary.trace_path);
  printf("  metrics: %s\n", summary.metrics_path);
  printf("  summary: %s\n", summary.summary_path);
  return 0;
}
