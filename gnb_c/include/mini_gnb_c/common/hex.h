#ifndef MINI_GNB_C_COMMON_HEX_H
#define MINI_GNB_C_COMMON_HEX_H

#include <stddef.h>
#include <stdint.h>

int mini_gnb_c_hex_to_bytes(const char* hex, uint8_t* out, size_t max_len, size_t* out_len);
int mini_gnb_c_bytes_to_hex(const uint8_t* bytes, size_t len, char* out, size_t out_size);

#endif
