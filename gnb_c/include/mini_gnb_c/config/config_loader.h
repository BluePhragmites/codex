#ifndef MINI_GNB_C_CONFIG_CONFIG_LOADER_H
#define MINI_GNB_C_CONFIG_CONFIG_LOADER_H

#include <stddef.h>

#include "mini_gnb_c/common/types.h"

int mini_gnb_c_load_config(const char* path,
                           mini_gnb_c_config_t* out_config,
                           char* error_message,
                           size_t error_message_size);

int mini_gnb_c_format_config_summary(const mini_gnb_c_config_t* config,
                                     char* out,
                                     size_t out_size);

#endif
