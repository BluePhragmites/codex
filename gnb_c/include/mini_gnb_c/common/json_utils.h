#ifndef MINI_GNB_C_COMMON_JSON_UTILS_H
#define MINI_GNB_C_COMMON_JSON_UTILS_H

#include <stddef.h>

char* mini_gnb_c_read_text_file(const char* path);
int mini_gnb_c_write_text_file(const char* path, const char* content);
int mini_gnb_c_ensure_directory(const char* path);
int mini_gnb_c_join_path(const char* left, const char* right, char* out, size_t out_size);

#endif
