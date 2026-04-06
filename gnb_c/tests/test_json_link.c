#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "test_helpers.h"

#include "mini_gnb_c/common/json_utils.h"
#include "mini_gnb_c/link/json_link.h"

void test_json_link_builds_stable_event_path(void) {
  char root_dir[MINI_GNB_C_MAX_PATH];
  char path[MINI_GNB_C_MAX_PATH];

  mini_gnb_c_make_output_dir("test_json_link", root_dir, sizeof(root_dir));
  mini_gnb_c_require(mini_gnb_c_json_link_build_event_path(root_dir,
                                                           "ue_to_gnb",
                                                           "ue",
                                                           7u,
                                                           "UL_NAS",
                                                           path,
                                                           sizeof(path)) == 0,
                     "expected JSON link path build");
  mini_gnb_c_require(strstr(path, "seq_000007_ue_UL_NAS.json") != NULL,
                     "expected stable event filename");
}

void test_json_link_emits_atomic_event_file(void) {
  char root_dir[MINI_GNB_C_MAX_PATH];
  char path[MINI_GNB_C_MAX_PATH];
  char tmp_path[MINI_GNB_C_MAX_PATH];
  char* file_text = NULL;
  struct stat st;

  mini_gnb_c_make_output_dir("test_json_link", root_dir, sizeof(root_dir));
  mini_gnb_c_require(mini_gnb_c_json_link_emit_event(root_dir,
                                                     "ue_to_gnb",
                                                     "ue",
                                                     "UL_NAS",
                                                     8u,
                                                     33,
                                                     "{\"hex\":\"7e00\"}",
                                                     path,
                                                     sizeof(path)) == 0,
                     "expected JSON link event emission");
  mini_gnb_c_require(stat(path, &st) == 0, "expected emitted JSON event file");

  file_text = mini_gnb_c_read_text_file(path);
  mini_gnb_c_require(file_text != NULL, "expected emitted JSON event text");
  mini_gnb_c_require(strstr(file_text, "\"channel\": \"ue_to_gnb\"") != NULL,
                     "expected channel in JSON event");
  mini_gnb_c_require(strstr(file_text, "\"source\": \"ue\"") != NULL, "expected source in JSON event");
  mini_gnb_c_require(strstr(file_text, "\"type\": \"UL_NAS\"") != NULL, "expected type in JSON event");
  mini_gnb_c_require(strstr(file_text, "\"hex\":\"7e00\"") != NULL, "expected payload in JSON event");

  if (snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.%ld", path, (long)getpid()) >= (int)sizeof(tmp_path)) {
    free(file_text);
    mini_gnb_c_require(false, "expected tmp path formatting");
  }
  mini_gnb_c_require(stat(tmp_path, &st) != 0, "expected no leftover tmp file after atomic rename");

  free(file_text);
}

void test_json_link_finds_event_by_sequence(void) {
  char root_dir[MINI_GNB_C_MAX_PATH];
  char path[MINI_GNB_C_MAX_PATH];
  char found_path[MINI_GNB_C_MAX_PATH];

  mini_gnb_c_make_output_dir("test_json_link_find", root_dir, sizeof(root_dir));
  mini_gnb_c_require(mini_gnb_c_json_link_emit_event(root_dir,
                                                     "ue_to_gnb_nas",
                                                     "ue",
                                                     "UL_NAS",
                                                     3u,
                                                     17,
                                                     "{\"c_rnti\":17921,\"nas_hex\":\"7E005C00\"}",
                                                     path,
                                                     sizeof(path)) == 0,
                     "expected JSON link event emission for find test");
  mini_gnb_c_require(mini_gnb_c_json_link_find_event_path(root_dir,
                                                          "ue_to_gnb_nas",
                                                          "ue",
                                                          3u,
                                                          found_path,
                                                          sizeof(found_path)) == 0,
                     "expected JSON link event lookup");
  mini_gnb_c_require(strcmp(path, found_path) == 0, "expected lookup to return emitted event path");
}
