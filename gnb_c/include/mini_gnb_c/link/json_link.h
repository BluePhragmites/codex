#ifndef MINI_GNB_C_LINK_JSON_LINK_H
#define MINI_GNB_C_LINK_JSON_LINK_H

#include <stddef.h>
#include <stdint.h>

#include "mini_gnb_c/common/types.h"

int mini_gnb_c_json_link_build_event_path(const char* root_dir,
                                          const char* channel,
                                          const char* source,
                                          uint32_t sequence,
                                          const char* type,
                                          char* out,
                                          size_t out_size);
int mini_gnb_c_json_link_find_event_path(const char* root_dir,
                                         const char* channel,
                                         const char* source,
                                         uint32_t sequence,
                                         char* out,
                                         size_t out_size);
int mini_gnb_c_json_link_write_event_file(const char* path,
                                          const char* channel,
                                          const char* source,
                                          const char* type,
                                          uint32_t sequence,
                                          int abs_slot,
                                          const char* payload_json);
int mini_gnb_c_json_link_emit_event(const char* root_dir,
                                    const char* channel,
                                    const char* source,
                                    const char* type,
                                    uint32_t sequence,
                                    int abs_slot,
                                    const char* payload_json,
                                    char* out_path,
                                    size_t out_path_size);

#endif
