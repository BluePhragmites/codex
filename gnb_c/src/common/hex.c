#include "mini_gnb_c/common/hex.h"

#include <ctype.h>
#include <stdio.h>

static int mini_gnb_c_hex_value(const char ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if (ch >= 'a' && ch <= 'f') {
    return 10 + (ch - 'a');
  }
  if (ch >= 'A' && ch <= 'F') {
    return 10 + (ch - 'A');
  }
  return -1;
}

int mini_gnb_c_hex_to_bytes(const char* hex, uint8_t* out, const size_t max_len, size_t* out_len) {
  size_t clean_len = 0;
  size_t byte_index = 0;
  int hi = -1;

  if (hex == NULL || out == NULL || out_len == NULL) {
    return -1;
  }

  *out_len = 0;

  for (size_t i = 0; hex[i] != '\0'; ++i) {
    const int value = mini_gnb_c_hex_value(hex[i]);
    if (value < 0) {
      continue;
    }

    if ((clean_len % 2U) == 0U) {
      hi = value;
    } else {
      if (byte_index >= max_len) {
        return -1;
      }
      out[byte_index++] = (uint8_t)((hi << 4) | value);
    }

    ++clean_len;
  }

  if ((clean_len % 2U) != 0U) {
    return -1;
  }

  *out_len = byte_index;
  return 0;
}

int mini_gnb_c_bytes_to_hex(const uint8_t* bytes, const size_t len, char* out, const size_t out_size) {
  if (out == NULL || out_size == 0U) {
    return -1;
  }

  if (bytes == NULL && len != 0U) {
    out[0] = '\0';
    return -1;
  }

  if (out_size < (len * 2U + 1U)) {
    out[0] = '\0';
    return -1;
  }

  for (size_t i = 0; i < len; ++i) {
    (void)snprintf(out + (i * 2U), out_size - (i * 2U), "%02X", bytes[i]);
  }
  out[len * 2U] = '\0';
  return 0;
}
