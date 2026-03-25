#include "mini_gnb_c/common/json_utils.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "mini_gnb_c/common/types.h"

char* mini_gnb_c_read_text_file(const char* path) {
  FILE* file = NULL;
  long size = 0;
  char* buffer = NULL;

  if (path == NULL) {
    return NULL;
  }

  file = fopen(path, "rb");
  if (file == NULL) {
    return NULL;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return NULL;
  }

  size = ftell(file);
  if (size < 0) {
    fclose(file);
    return NULL;
  }

  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return NULL;
  }

  buffer = (char*)malloc((size_t)size + 1U);
  if (buffer == NULL) {
    fclose(file);
    return NULL;
  }

  if ((size_t)size != fread(buffer, 1U, (size_t)size, file)) {
    fclose(file);
    free(buffer);
    return NULL;
  }

  buffer[size] = '\0';
  fclose(file);
  return buffer;
}

static int mini_gnb_c_ensure_parent_directories(const char* path) {
  char temp[MINI_GNB_C_MAX_PATH];

  if (path == NULL) {
    return -1;
  }

  (void)snprintf(temp, sizeof(temp), "%s", path);
  for (size_t i = 1; temp[i] != '\0'; ++i) {
    if (temp[i] == '/' || temp[i] == '\\') {
      const char saved = temp[i];
      temp[i] = '\0';
      if (strlen(temp) > 0U) {
        (void)mkdir(temp, 0777);
      }
      temp[i] = saved;
    }
  }
  return 0;
}

int mini_gnb_c_write_text_file(const char* path, const char* content) {
  FILE* file = NULL;

  if (path == NULL || content == NULL) {
    return -1;
  }

  (void)mini_gnb_c_ensure_parent_directories(path);
  file = fopen(path, "wb");
  if (file == NULL) {
    return -1;
  }

  if (fwrite(content, 1U, strlen(content), file) != strlen(content)) {
    fclose(file);
    return -1;
  }

  fclose(file);
  return 0;
}

int mini_gnb_c_ensure_directory(const char* path) {
  if (path == NULL) {
    return -1;
  }

  if (mkdir(path, 0777) == 0 || errno == EEXIST) {
    return 0;
  }
  return -1;
}

int mini_gnb_c_join_path(const char* left, const char* right, char* out, const size_t out_size) {
  const char* separator = "";
  size_t left_len = 0;

  if (left == NULL || right == NULL || out == NULL || out_size == 0U) {
    return -1;
  }

  left_len = strlen(left);
  if (left_len > 0U && left[left_len - 1U] != '/' && left[left_len - 1U] != '\\') {
    separator = "/";
  }

  if (snprintf(out, out_size, "%s%s%s", left, separator, right) >= (int)out_size) {
    return -1;
  }
  return 0;
}
