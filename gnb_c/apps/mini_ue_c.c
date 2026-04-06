#include <stdio.h>
#include <string.h>

#include "mini_gnb_c/common/json_utils.h"
#include "mini_gnb_c/config/config_loader.h"
#include "mini_gnb_c/link/json_link.h"
#include "mini_gnb_c/ue/mini_ue_fsm.h"
#include "mini_gnb_c/ue/mini_ue_runtime.h"

#ifndef MINI_GNB_C_SOURCE_DIR
#define MINI_GNB_C_SOURCE_DIR "."
#endif

static bool mini_gnb_c_path_is_absolute(const char* path) {
  if (path == NULL || path[0] == '\0') {
    return false;
  }
  if (path[0] == '/' || path[0] == '\\') {
    return true;
  }
  return strlen(path) > 1u && path[1] == ':';
}

static int mini_gnb_c_resolve_exchange_dir(const char* configured_path, char* out, const size_t out_size) {
  if (configured_path == NULL || configured_path[0] == '\0' || out == NULL || out_size == 0u) {
    return -1;
  }
  if (mini_gnb_c_path_is_absolute(configured_path)) {
    return snprintf(out, out_size, "%s", configured_path) < (int)out_size ? 0 : -1;
  }
  return mini_gnb_c_join_path(MINI_GNB_C_SOURCE_DIR, configured_path, out, out_size);
}

int main(int argc, char** argv) {
  const char* config_path = argc > 1 ? argv[1] : MINI_GNB_C_SOURCE_DIR "/config/default_cell.yml";
  char error_message[256];
  mini_gnb_c_config_t config;
  char exchange_dir[MINI_GNB_C_MAX_PATH];
  mini_gnb_c_mini_ue_fsm_t fsm;
  mini_gnb_c_ue_event_t event;
  int emitted_count = 0;

  if (mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) != 0) {
    fprintf(stderr, "failed to load config %s: %s\n", config_path, error_message);
    return 1;
  }
  if (config.sim.shared_slot_path[0] != '\0') {
    char shared_slot_path[MINI_GNB_C_MAX_PATH];

    if (mini_gnb_c_resolve_exchange_dir(config.sim.shared_slot_path, shared_slot_path, sizeof(shared_slot_path)) != 0) {
      fprintf(stderr, "missing or invalid sim.shared_slot_path in %s\n", config_path);
      return 1;
    }
    (void)snprintf(config.sim.shared_slot_path, sizeof(config.sim.shared_slot_path), "%s", shared_slot_path);
    if (mini_gnb_c_run_shared_ue_runtime(&config) != 0) {
      fprintf(stderr, "shared-slot UE runtime failed for %s\n", config_path);
      return 1;
    }
    printf("UE shared-slot runtime finished. shared_slot_path=%s\n", config.sim.shared_slot_path);
    return 0;
  }
  if (mini_gnb_c_resolve_exchange_dir(config.sim.local_exchange_dir, exchange_dir, sizeof(exchange_dir)) != 0) {
    fprintf(stderr, "missing or invalid sim.local_exchange_dir in %s\n", config_path);
    return 1;
  }

  mini_gnb_c_mini_ue_fsm_init(&fsm, &config.sim);
  while (mini_gnb_c_mini_ue_fsm_next_event(&fsm, &event) > 0) {
    char payload_json[2048];
    char emitted_path[MINI_GNB_C_MAX_PATH];

    if (mini_gnb_c_ue_event_build_payload_json(&event, payload_json, sizeof(payload_json)) != 0) {
      fprintf(stderr, "failed to format UE event payload for %s\n", mini_gnb_c_ue_event_type_to_string(event.type));
      return 1;
    }
    if (mini_gnb_c_json_link_emit_event(exchange_dir,
                                        "ue_to_gnb",
                                        "ue",
                                        mini_gnb_c_ue_event_type_to_string(event.type),
                                        event.sequence,
                                        event.abs_slot,
                                        payload_json,
                                        emitted_path,
                                        sizeof(emitted_path)) != 0) {
      fprintf(stderr, "failed to emit UE event %s\n", mini_gnb_c_ue_event_type_to_string(event.type));
      return 1;
    }

    ++emitted_count;
    printf("Emitted UE event seq=%u type=%s abs_slot=%d path=%s\n",
           event.sequence,
           mini_gnb_c_ue_event_type_to_string(event.type),
           event.abs_slot,
           emitted_path);
  }

  printf("UE plan complete. emitted_events=%d exchange_dir=%s\n", emitted_count, exchange_dir);
  return 0;
}
