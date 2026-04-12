#include <stdio.h>

#include "mini_gnb_c/common/simulator.h"
#include "mini_gnb_c/radio/b210_app_runtime.h"

int main(int argc, char** argv) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char output_dir[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  char config_summary[4096];
  mini_gnb_c_config_t config;
  mini_gnb_c_simulator_t simulator;
  mini_gnb_c_run_summary_t summary;

  if (argc > 1 && argv != NULL && argv[1] != NULL && argv[1][0] != '\0') {
    if (snprintf(config_path, sizeof(config_path), "%s", argv[1]) >= (int)sizeof(config_path)) {
      fprintf(stderr, "failed to copy config path\n");
      return 1;
    }
  } else {
    if (snprintf(config_path,
                 sizeof(config_path),
                 "%s/config/default_cell.yml",
                 MINI_GNB_C_SOURCE_DIR) >= (int)sizeof(config_path)) {
      fprintf(stderr, "failed to construct config path\n");
      return 1;
    }
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
  if (mini_gnb_c_rf_app_runtime_requested(&config)) {
    if (mini_gnb_c_b210_app_runtime_run("mini_gnb_c_sim", &config, error_message, sizeof(error_message)) != 0) {
      fprintf(stderr, "B210 app runtime failed: %s\n", error_message);
      return 1;
    }
    return 0;
  }
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
  printf("  PUCCH SR detected: %llu\n", (unsigned long long)summary.counters.pucch_sr_detect_ok);
  printf("  UL BSR RX OK: %llu\n", (unsigned long long)summary.counters.ul_bsr_rx_ok);
  printf("  DL data sent: %llu\n", (unsigned long long)summary.counters.dl_data_sent);
  printf("  UL data RX OK: %llu\n", (unsigned long long)summary.counters.ul_data_rx_ok);
  printf("Artifacts:\n");
  printf("  trace: %s\n", summary.trace_path);
  printf("  metrics: %s\n", summary.metrics_path);
  printf("  summary: %s\n", summary.summary_path);
  if (summary.ngap_trace_pcap_path[0] != '\0') {
    printf("  ngap_pcap: %s\n", summary.ngap_trace_pcap_path);
  }
  if (summary.gtpu_trace_pcap_path[0] != '\0') {
    printf("  gtpu_pcap: %s\n", summary.gtpu_trace_pcap_path);
  }
  return 0;
}
