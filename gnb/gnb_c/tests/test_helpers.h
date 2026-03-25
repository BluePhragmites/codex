#ifndef MINI_GNB_C_TESTS_TEST_HELPERS_H
#define MINI_GNB_C_TESTS_TEST_HELPERS_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

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
  (void)snprintf(out, out_size, "%s/config/default_cell.json", MINI_GNB_C_SOURCE_DIR);
}

static inline void mini_gnb_c_make_output_dir(const char* name, char* out, size_t out_size) {
  if (out == NULL || out_size == 0U) {
    return;
  }
  (void)snprintf(out, out_size, "%s/out/%s", MINI_GNB_C_SOURCE_DIR, name);
}

#endif
