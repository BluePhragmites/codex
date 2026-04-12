#ifndef MINI_GNB_C_RADIO_B210_APP_RUNTIME_H
#define MINI_GNB_C_RADIO_B210_APP_RUNTIME_H

#include <stddef.h>
#include <stdbool.h>

#include "mini_gnb_c/config/config_loader.h"

bool mini_gnb_c_rf_app_runtime_requested(const mini_gnb_c_config_t* config);

int mini_gnb_c_b210_app_runtime_run(const char* app_name,
                                    const mini_gnb_c_config_t* config,
                                    char* error_message,
                                    size_t error_message_size);

#endif
