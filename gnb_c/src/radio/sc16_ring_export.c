#include "mini_gnb_c/radio/sc16_ring_export.h"

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MINI_GNB_C_SC16_RING_EXPORT_MAX_CHANNELS 2u

static int mini_gnb_c_sc16_ring_export_fail(char* error_message,
                                            const size_t error_message_size,
                                            const char* message) {
  if (error_message != NULL && error_message_size > 0u) {
    (void)snprintf(error_message, error_message_size, "%s", message != NULL ? message : "unknown error");
  }
  return -1;
}

static int mini_gnb_c_sc16_ring_export_failf(char* error_message,
                                             const size_t error_message_size,
                                             const char* prefix,
                                             const char* detail) {
  if (error_message != NULL && error_message_size > 0u) {
    (void)snprintf(error_message,
                   error_message_size,
                   "%s%s%s",
                   prefix != NULL ? prefix : "error",
                   (detail != NULL && detail[0] != '\0') ? ": " : "",
                   detail != NULL ? detail : "");
  }
  return -1;
}

static int mini_gnb_c_sc16_ring_export_ensure_directory_recursive(const char* path) {
  char temp[MINI_GNB_C_MAX_PATH];
  size_t len = 0u;
  size_t i = 0u;

  if (path == NULL || path[0] == '\0') {
    return -1;
  }
  if (snprintf(temp, sizeof(temp), "%s", path) >= (int)sizeof(temp)) {
    return -1;
  }
  len = strlen(temp);
  if (len == 0u) {
    return -1;
  }

  for (i = 1u; i < len; ++i) {
    if (temp[i] == '/') {
      temp[i] = '\0';
      if (strlen(temp) > 0u && mkdir(temp, 0777) != 0 && errno != EEXIST) {
        return -1;
      }
      temp[i] = '/';
    }
  }
  if (mkdir(temp, 0777) != 0 && errno != EEXIST) {
    return -1;
  }
  return 0;
}

static int mini_gnb_c_sc16_ring_export_ensure_parent_directory(const char* path) {
  char parent[MINI_GNB_C_MAX_PATH];
  char* slash = NULL;

  if (path == NULL || path[0] == '\0') {
    return -1;
  }
  if (snprintf(parent, sizeof(parent), "%s", path) >= (int)sizeof(parent)) {
    return -1;
  }
  slash = strrchr(parent, '/');
  if (slash == NULL || slash == parent) {
    return 0;
  }
  *slash = '\0';
  return mini_gnb_c_sc16_ring_export_ensure_directory_recursive(parent);
}

int mini_gnb_c_sc16_ring_export_range(const mini_gnb_c_sc16_ring_map_t* ring,
                                      const uint64_t seq_start,
                                      const uint64_t seq_end,
                                      const char* output_prefix,
                                      mini_gnb_c_sc16_ring_export_report_t* report,
                                      char* error_message,
                                      const size_t error_message_size) {
  FILE* channel_files[MINI_GNB_C_SC16_RING_EXPORT_MAX_CHANNELS] = {NULL, NULL};
  FILE* metadata_fp = NULL;
  char channel_path[MINI_GNB_C_SC16_RING_EXPORT_MAX_CHANNELS][MINI_GNB_C_MAX_PATH];
  char metadata_path[MINI_GNB_C_MAX_PATH];
  uint64_t seq = 0u;
  uint32_t channel_index = 0u;
  uint64_t samples_per_channel = 0u;
  int rc = -1;

  if (ring == NULL || ring->superblock == NULL || output_prefix == NULL || output_prefix[0] == '\0' || report == NULL) {
    return mini_gnb_c_sc16_ring_export_fail(error_message, error_message_size, "invalid ring export arguments");
  }
  if (seq_end < seq_start) {
    return mini_gnb_c_sc16_ring_export_fail(error_message, error_message_size, "seq_end must be greater than or equal to seq_start");
  }
  if (!mini_gnb_c_sc16_ring_map_seq_valid(ring, seq_start) || !mini_gnb_c_sc16_ring_map_seq_valid(ring, seq_end)) {
    return mini_gnb_c_sc16_ring_export_fail(error_message,
                                            error_message_size,
                                            "requested seq range is not fully available in the current ring window");
  }
  memset(report, 0, sizeof(*report));
  report->seq_start = seq_start;
  report->seq_end = seq_end;
  report->channel_count = ring->superblock->channel_count;
  report->sample_rate_sps = ring->superblock->sample_rate_sps;

  for (channel_index = 0u; channel_index < ring->superblock->channel_count; ++channel_index) {
    if ((size_t)snprintf(channel_path[channel_index],
                         sizeof(channel_path[channel_index]),
                         "%s_ch%u.sc16",
                         output_prefix,
                         channel_index) >= sizeof(channel_path[channel_index])) {
      rc = mini_gnb_c_sc16_ring_export_fail(error_message, error_message_size, "channel export path is too long");
      goto cleanup;
    }
    if (mini_gnb_c_sc16_ring_export_ensure_parent_directory(channel_path[channel_index]) != 0) {
      rc = mini_gnb_c_sc16_ring_export_failf(error_message,
                                             error_message_size,
                                             "failed to prepare export directory",
                                             channel_path[channel_index]);
      goto cleanup;
    }
    channel_files[channel_index] = fopen(channel_path[channel_index], "wb");
    if (channel_files[channel_index] == NULL) {
      rc = mini_gnb_c_sc16_ring_export_failf(error_message,
                                             error_message_size,
                                             "failed to open export file",
                                             channel_path[channel_index]);
      goto cleanup;
    }
  }

  for (seq = seq_start; seq <= seq_end; ++seq) {
    const mini_gnb_c_sc16_ring_map_descriptor_t* descriptor = mini_gnb_c_sc16_ring_map_get_descriptor(ring, seq);

    if (descriptor == NULL) {
      rc = mini_gnb_c_sc16_ring_export_fail(error_message, error_message_size, "requested seq range contains a missing descriptor");
      goto cleanup;
    }
    for (channel_index = 0u; channel_index < ring->superblock->channel_count; ++channel_index) {
      const int16_t* payload = mini_gnb_c_sc16_ring_map_get_channel_payload(ring, seq, channel_index);

      if (payload == NULL) {
        rc = mini_gnb_c_sc16_ring_export_fail(error_message, error_message_size, "failed to resolve channel payload during export");
        goto cleanup;
      }
      if (fwrite(payload, sizeof(int16_t) * 2u, descriptor->sample_count, channel_files[channel_index]) != descriptor->sample_count) {
        rc = mini_gnb_c_sc16_ring_export_fail(error_message, error_message_size, "failed to write channel export data");
        goto cleanup;
      }
    }
    report->blocks_exported += 1u;
    samples_per_channel += descriptor->sample_count;
  }
  report->samples_per_channel = samples_per_channel;

  if ((size_t)snprintf(metadata_path, sizeof(metadata_path), "%s_meta.txt", output_prefix) >= sizeof(metadata_path)) {
    rc = mini_gnb_c_sc16_ring_export_fail(error_message, error_message_size, "metadata export path is too long");
    goto cleanup;
  }
  if (mini_gnb_c_sc16_ring_export_ensure_parent_directory(metadata_path) != 0) {
    rc = mini_gnb_c_sc16_ring_export_failf(error_message, error_message_size, "failed to prepare metadata directory", metadata_path);
    goto cleanup;
  }
  metadata_fp = fopen(metadata_path, "w");
  if (metadata_fp == NULL) {
    rc = mini_gnb_c_sc16_ring_export_failf(error_message, error_message_size, "failed to open metadata file", metadata_path);
    goto cleanup;
  }
  fprintf(metadata_fp, "ring_path=%s\n", ring->path[0] != '\0' ? ring->path : "(mapped)");
  fprintf(metadata_fp, "seq_start=%" PRIu64 "\n", report->seq_start);
  fprintf(metadata_fp, "seq_end=%" PRIu64 "\n", report->seq_end);
  fprintf(metadata_fp, "blocks_exported=%" PRIu64 "\n", report->blocks_exported);
  fprintf(metadata_fp, "samples_per_channel=%" PRIu64 "\n", report->samples_per_channel);
  fprintf(metadata_fp, "channel_count=%u\n", report->channel_count);
  fprintf(metadata_fp, "sample_rate_sps=%" PRIu64 "\n", report->sample_rate_sps);
  for (channel_index = 0u; channel_index < ring->superblock->channel_count; ++channel_index) {
    fprintf(metadata_fp, "channel_%u_file=%s\n", channel_index, channel_path[channel_index]);
  }
  rc = 0;

cleanup:
  if (metadata_fp != NULL) {
    fclose(metadata_fp);
  }
  for (channel_index = 0u; channel_index < MINI_GNB_C_SC16_RING_EXPORT_MAX_CHANNELS; ++channel_index) {
    if (channel_files[channel_index] != NULL) {
      fclose(channel_files[channel_index]);
    }
  }
  return rc;
}
