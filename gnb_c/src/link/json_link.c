#include "mini_gnb_c/link/json_link.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mini_gnb_c/common/json_utils.h"

static int mini_gnb_c_json_link_escape_string(const char* text, char* out, const size_t out_size) {
  size_t read_index = 0u;
  size_t write_index = 0u;

  if (text == NULL || out == NULL || out_size == 0u) {
    return -1;
  }

  while (text[read_index] != '\0') {
    const char ch = text[read_index++];

    if ((ch == '\\' || ch == '"') && write_index + 2u < out_size) {
      out[write_index++] = '\\';
      out[write_index++] = ch;
      continue;
    }
    if (ch == '\n' && write_index + 2u < out_size) {
      out[write_index++] = '\\';
      out[write_index++] = 'n';
      continue;
    }
    if (ch == '\r' && write_index + 2u < out_size) {
      out[write_index++] = '\\';
      out[write_index++] = 'r';
      continue;
    }
    if (ch == '\t' && write_index + 2u < out_size) {
      out[write_index++] = '\\';
      out[write_index++] = 't';
      continue;
    }
    if (write_index + 1u >= out_size) {
      return -1;
    }
    out[write_index++] = ch;
  }

  out[write_index] = '\0';
  return 0;
}

int mini_gnb_c_json_link_build_event_path(const char* root_dir,
                                          const char* channel,
                                          const char* source,
                                          const uint32_t sequence,
                                          const char* type,
                                          char* out,
                                          const size_t out_size) {
  char channel_dir[MINI_GNB_C_MAX_PATH];
  char file_name[MINI_GNB_C_MAX_PATH];

  if (root_dir == NULL || channel == NULL || source == NULL || type == NULL || out == NULL || out_size == 0u) {
    return -1;
  }
  if (mini_gnb_c_join_path(root_dir, channel, channel_dir, sizeof(channel_dir)) != 0) {
    return -1;
  }
  if (snprintf(file_name, sizeof(file_name), "seq_%06u_%s_%s.json", sequence, source, type) >= (int)sizeof(file_name)) {
    return -1;
  }

  return mini_gnb_c_join_path(channel_dir, file_name, out, out_size);
}

int mini_gnb_c_json_link_write_event_file(const char* path,
                                          const char* channel,
                                          const char* source,
                                          const char* type,
                                          const uint32_t sequence,
                                          const int abs_slot,
                                          const char* payload_json) {
  char escaped_channel[MINI_GNB_C_MAX_TEXT];
  char escaped_source[MINI_GNB_C_MAX_TEXT];
  char escaped_type[MINI_GNB_C_MAX_TEXT];
  char content[2048];
  char tmp_path[MINI_GNB_C_MAX_PATH];

  if (path == NULL || channel == NULL || source == NULL || type == NULL) {
    return -1;
  }
  if (mini_gnb_c_json_link_escape_string(channel, escaped_channel, sizeof(escaped_channel)) != 0 ||
      mini_gnb_c_json_link_escape_string(source, escaped_source, sizeof(escaped_source)) != 0 ||
      mini_gnb_c_json_link_escape_string(type, escaped_type, sizeof(escaped_type)) != 0) {
    return -1;
  }
  if (snprintf(content,
               sizeof(content),
               "{\n"
               "  \"sequence\": %u,\n"
               "  \"abs_slot\": %d,\n"
               "  \"channel\": \"%s\",\n"
               "  \"source\": \"%s\",\n"
               "  \"type\": \"%s\",\n"
               "  \"payload\": %s\n"
               "}\n",
               sequence,
               abs_slot,
               escaped_channel,
               escaped_source,
               escaped_type,
               payload_json != NULL ? payload_json : "null") >= (int)sizeof(content)) {
    return -1;
  }
  if (snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.%ld", path, (long)getpid()) >= (int)sizeof(tmp_path)) {
    return -1;
  }
  if (mini_gnb_c_write_text_file(tmp_path, content) != 0) {
    return -1;
  }
  if (rename(tmp_path, path) != 0) {
    (void)remove(tmp_path);
    return -1;
  }

  return 0;
}

int mini_gnb_c_json_link_emit_event(const char* root_dir,
                                    const char* channel,
                                    const char* source,
                                    const char* type,
                                    const uint32_t sequence,
                                    const int abs_slot,
                                    const char* payload_json,
                                    char* out_path,
                                    const size_t out_path_size) {
  char path[MINI_GNB_C_MAX_PATH];

  if (mini_gnb_c_json_link_build_event_path(root_dir,
                                            channel,
                                            source,
                                            sequence,
                                            type,
                                            path,
                                            sizeof(path)) != 0) {
    return -1;
  }
  if (mini_gnb_c_json_link_write_event_file(path,
                                            channel,
                                            source,
                                            type,
                                            sequence,
                                            abs_slot,
                                            payload_json) != 0) {
    return -1;
  }
  if (out_path != NULL && out_path_size > 0u &&
      snprintf(out_path, out_path_size, "%s", path) >= (int)out_path_size) {
    return -1;
  }

  return 0;
}
