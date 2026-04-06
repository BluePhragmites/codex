#ifndef MINI_GNB_C_TESTS_TEST_HELPERS_H
#define MINI_GNB_C_TESTS_TEST_HELPERS_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "mini_gnb_c/common/types.h"

static inline void mini_gnb_c_require(bool condition, const char* message) {
  if (!condition) {
    fprintf(stderr, "[FAIL] %s\n", message);
    exit(1);
  }
}

static inline void mini_gnb_c_default_config_path(char* out, size_t out_size) {
  if (out == NULL || out_size == 0U) {
    return;
  }
  (void)snprintf(out, out_size, "%s/config/default_cell.yml", MINI_GNB_C_SOURCE_DIR);
}

static inline void mini_gnb_c_make_output_dir(const char* name, char* out, size_t out_size) {
  static unsigned int sequence = 0U;
  char command[MINI_GNB_C_MAX_PATH + 32U];

  if (out == NULL || out_size == 0U) {
    return;
  }
  ++sequence;
  (void)snprintf(out, out_size, "%s/out/%s_%ld_%u", MINI_GNB_C_SOURCE_DIR, name, (long)getpid(), sequence);
  (void)snprintf(command, sizeof(command), "mkdir -p '%s'", out);
  mini_gnb_c_require(system(command) == 0, "expected unique test output directory");
}

static inline void mini_gnb_c_reset_test_dir(const char* path) {
  char command[MINI_GNB_C_MAX_PATH + 32U];

  mini_gnb_c_require(path != NULL, "expected test directory path");
  (void)snprintf(command, sizeof(command), "mkdir -p '%s'", path);
  mini_gnb_c_require(system(command) == 0, "expected test subdirectory");
}

#endif
