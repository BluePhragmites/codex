#define _POSIX_C_SOURCE 200809L

#include "mini_gnb_c/radio/sc16_ring_map.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MINI_GNB_C_SC16_RING_SAMPLE_FORMAT_SC16 1u

static int mini_gnb_c_sc16_ring_map_fail(char* error_message,
                                         const size_t error_message_size,
                                         const char* message) {
  if (error_message != NULL && error_message_size > 0u) {
    (void)snprintf(error_message, error_message_size, "%s", message != NULL ? message : "unknown error");
  }
  return -1;
}

static int mini_gnb_c_sc16_ring_map_failf(char* error_message,
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

static size_t mini_gnb_c_sc16_ring_map_payload_block_bytes(const uint32_t block_samples) {
  return (size_t)block_samples * sizeof(int16_t) * 2u;
}

static size_t mini_gnb_c_sc16_ring_map_total_bytes(const mini_gnb_c_sc16_ring_map_config_t* config) {
  const size_t descriptor_bytes =
      (size_t)config->block_count * sizeof(mini_gnb_c_sc16_ring_map_descriptor_t);
  const size_t payload_bytes =
      (size_t)config->block_count * mini_gnb_c_sc16_ring_map_payload_block_bytes(config->block_samples) * config->channel_count;
  return sizeof(mini_gnb_c_sc16_ring_map_superblock_t) + descriptor_bytes + payload_bytes;
}

static int mini_gnb_c_sc16_ring_map_init_view(mini_gnb_c_sc16_ring_map_t* ring,
                                              const bool writable,
                                              void* mapping,
                                              const size_t mapped_size,
                                              const int fd,
                                              const char* path,
                                              char* error_message,
                                              const size_t error_message_size) {
  mini_gnb_c_sc16_ring_map_superblock_t* superblock = NULL;

  if (ring == NULL || mapping == NULL || mapped_size < sizeof(mini_gnb_c_sc16_ring_map_superblock_t)) {
    return mini_gnb_c_sc16_ring_map_fail(error_message, error_message_size, "invalid ring map view");
  }
  superblock = (mini_gnb_c_sc16_ring_map_superblock_t*)mapping;
  if (superblock->magic != MINI_GNB_C_SC16_RING_MAP_MAGIC ||
      superblock->version != MINI_GNB_C_SC16_RING_MAP_VERSION ||
      superblock->sample_format != MINI_GNB_C_SC16_RING_SAMPLE_FORMAT_SC16 ||
      superblock->channel_count == 0u) {
    return mini_gnb_c_sc16_ring_map_fail(error_message, error_message_size, "invalid sc16 ring map header");
  }
  if (superblock->mapped_size != mapped_size ||
      superblock->descriptor_offset != sizeof(mini_gnb_c_sc16_ring_map_superblock_t) ||
      superblock->payload_offset !=
          sizeof(mini_gnb_c_sc16_ring_map_superblock_t) +
              ((uint64_t)superblock->block_count * sizeof(mini_gnb_c_sc16_ring_map_descriptor_t))) {
    return mini_gnb_c_sc16_ring_map_fail(error_message, error_message_size, "corrupt sc16 ring map layout");
  }
  memset(ring, 0, sizeof(*ring));
  ring->fd = fd;
  ring->mapped_size = mapped_size;
  ring->writable = writable;
  ring->mapping = mapping;
  ring->superblock = superblock;
  ring->descriptors = (mini_gnb_c_sc16_ring_map_descriptor_t*)((uint8_t*)mapping + superblock->descriptor_offset);
  ring->payload_base = (uint8_t*)mapping + superblock->payload_offset;
  if (path != NULL) {
    (void)snprintf(ring->path, sizeof(ring->path), "%s", path);
  }
  return 0;
}

void mini_gnb_c_sc16_ring_map_config_init(mini_gnb_c_sc16_ring_map_config_t* config) {
  if (config == NULL) {
    return;
  }
  memset(config, 0, sizeof(*config));
  config->role = MINI_GNB_C_SC16_RING_ROLE_RX;
  config->channel_count = 1u;
  config->block_count = 1024u;
  config->block_samples = 4096u;
  config->sample_rate_sps = 20000000u;
}

int mini_gnb_c_sc16_ring_map_create(const char* path,
                                    const mini_gnb_c_sc16_ring_map_config_t* config,
                                    mini_gnb_c_sc16_ring_map_t* ring,
                                    char* error_message,
                                    const size_t error_message_size) {
  mini_gnb_c_sc16_ring_map_superblock_t* superblock = NULL;
  mini_gnb_c_sc16_ring_map_descriptor_t* descriptors = NULL;
  void* mapping = MAP_FAILED;
  size_t mapped_size = 0u;
  int fd = -1;
  size_t i = 0u;

  if (path == NULL || path[0] == '\0' || config == NULL || ring == NULL) {
    return mini_gnb_c_sc16_ring_map_fail(error_message, error_message_size, "invalid ring map create arguments");
  }
  if (config->channel_count == 0u || config->block_count == 0u || config->block_samples == 0u ||
      config->sample_rate_sps == 0u) {
    return mini_gnb_c_sc16_ring_map_fail(error_message, error_message_size, "invalid ring map geometry");
  }
  mapped_size = mini_gnb_c_sc16_ring_map_total_bytes(config);
  fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0666);
  if (fd < 0) {
    return mini_gnb_c_sc16_ring_map_failf(error_message, error_message_size, "failed to create ring map", path);
  }
  if (ftruncate(fd, (off_t)mapped_size) != 0) {
    int rc = mini_gnb_c_sc16_ring_map_failf(error_message, error_message_size, "failed to resize ring map", path);

    close(fd);
    return rc;
  }
  mapping = mmap(NULL, mapped_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mapping == MAP_FAILED) {
    int rc = mini_gnb_c_sc16_ring_map_fail(error_message, error_message_size, "failed to mmap ring map");

    close(fd);
    return rc;
  }
  memset(mapping, 0, mapped_size);

  superblock = (mini_gnb_c_sc16_ring_map_superblock_t*)mapping;
  superblock->magic = MINI_GNB_C_SC16_RING_MAP_MAGIC;
  superblock->version = MINI_GNB_C_SC16_RING_MAP_VERSION;
  superblock->role = (uint32_t)config->role;
  superblock->sample_format = MINI_GNB_C_SC16_RING_SAMPLE_FORMAT_SC16;
  superblock->channel_count = config->channel_count;
  superblock->block_count = config->block_count;
  superblock->block_samples = config->block_samples;
  superblock->block_bytes =
      (uint32_t)(mini_gnb_c_sc16_ring_map_payload_block_bytes(config->block_samples) * config->channel_count);
  superblock->sample_rate_sps = config->sample_rate_sps;
  superblock->mapped_size = mapped_size;
  superblock->descriptor_offset = sizeof(*superblock);
  superblock->payload_offset =
      sizeof(*superblock) + ((uint64_t)config->block_count * sizeof(mini_gnb_c_sc16_ring_map_descriptor_t));
  superblock->oldest_valid_seq = 0u;
  superblock->next_write_seq = 0u;
  superblock->last_committed_seq = UINT64_MAX;
  superblock->dropped_blocks = 0u;

  descriptors = (mini_gnb_c_sc16_ring_map_descriptor_t*)((uint8_t*)mapping + superblock->descriptor_offset);
  for (i = 0u; i < config->block_count; ++i) {
    descriptors[i].seqno = UINT64_MAX;
    descriptors[i].payload_offset = superblock->payload_offset + ((uint64_t)i * superblock->block_bytes);
    descriptors[i].state = MINI_GNB_C_SC16_RING_BLOCK_EMPTY;
  }

  if (mini_gnb_c_sc16_ring_map_init_view(ring,
                                         true,
                                         mapping,
                                         mapped_size,
                                         fd,
                                         path,
                                         error_message,
                                         error_message_size) != 0) {
    munmap(mapping, mapped_size);
    close(fd);
    return -1;
  }
  return 0;
}

int mini_gnb_c_sc16_ring_map_open_existing(const char* path,
                                           const bool writable,
                                           mini_gnb_c_sc16_ring_map_t* ring,
                                           char* error_message,
                                           const size_t error_message_size) {
  struct stat st;
  void* mapping = MAP_FAILED;
  int fd = -1;
  int open_flags = writable ? O_RDWR : O_RDONLY;
  int prot = PROT_READ | (writable ? PROT_WRITE : 0);

  if (path == NULL || path[0] == '\0' || ring == NULL) {
    return mini_gnb_c_sc16_ring_map_fail(error_message, error_message_size, "invalid ring map open arguments");
  }
  fd = open(path, open_flags);
  if (fd < 0) {
    return mini_gnb_c_sc16_ring_map_failf(error_message, error_message_size, "failed to open ring map", path);
  }
  if (fstat(fd, &st) != 0 || st.st_size <= 0) {
    close(fd);
    return mini_gnb_c_sc16_ring_map_failf(error_message, error_message_size, "failed to stat ring map", path);
  }
  mapping = mmap(NULL, (size_t)st.st_size, prot, MAP_SHARED, fd, 0);
  if (mapping == MAP_FAILED) {
    close(fd);
    return mini_gnb_c_sc16_ring_map_fail(error_message, error_message_size, "failed to mmap ring map");
  }
  if (mini_gnb_c_sc16_ring_map_init_view(ring,
                                         writable,
                                         mapping,
                                         (size_t)st.st_size,
                                         fd,
                                         path,
                                         error_message,
                                         error_message_size) != 0) {
    munmap(mapping, (size_t)st.st_size);
    close(fd);
    return -1;
  }
  return 0;
}

void mini_gnb_c_sc16_ring_map_close(mini_gnb_c_sc16_ring_map_t* ring) {
  if (ring == NULL) {
    return;
  }
  if (ring->mapping != NULL && ring->mapped_size > 0u) {
    if (ring->writable) {
      (void)msync(ring->mapping, ring->mapped_size, MS_SYNC);
    }
    (void)munmap(ring->mapping, ring->mapped_size);
  }
  if (ring->fd >= 0) {
    (void)close(ring->fd);
  }
  memset(ring, 0, sizeof(*ring));
  ring->fd = -1;
}

size_t mini_gnb_c_sc16_ring_map_slot_for_seq(const mini_gnb_c_sc16_ring_map_t* ring, const uint64_t seqno) {
  if (ring == NULL || ring->superblock == NULL || ring->superblock->block_count == 0u) {
    return 0u;
  }
  return (size_t)(seqno % ring->superblock->block_count);
}

bool mini_gnb_c_sc16_ring_map_seq_valid(const mini_gnb_c_sc16_ring_map_t* ring, const uint64_t seqno) {
  const mini_gnb_c_sc16_ring_map_superblock_t* sb = NULL;

  if (ring == NULL || ring->superblock == NULL) {
    return false;
  }
  sb = ring->superblock;
  if (sb->last_committed_seq == UINT64_MAX || sb->next_write_seq == 0u) {
    return false;
  }
  return seqno >= sb->oldest_valid_seq && seqno < sb->next_write_seq;
}

const mini_gnb_c_sc16_ring_map_descriptor_t* mini_gnb_c_sc16_ring_map_get_descriptor(
    const mini_gnb_c_sc16_ring_map_t* ring, const uint64_t seqno) {
  size_t slot = 0u;
  const mini_gnb_c_sc16_ring_map_descriptor_t* descriptor = NULL;

  if (!mini_gnb_c_sc16_ring_map_seq_valid(ring, seqno)) {
    return NULL;
  }
  slot = mini_gnb_c_sc16_ring_map_slot_for_seq(ring, seqno);
  descriptor = &ring->descriptors[slot];
  if (descriptor->seqno != seqno || descriptor->state != MINI_GNB_C_SC16_RING_BLOCK_READY) {
    return NULL;
  }
  return descriptor;
}

const int16_t* mini_gnb_c_sc16_ring_map_get_payload(const mini_gnb_c_sc16_ring_map_t* ring, const uint64_t seqno) {
  const mini_gnb_c_sc16_ring_map_descriptor_t* descriptor = mini_gnb_c_sc16_ring_map_get_descriptor(ring, seqno);

  if (descriptor == NULL || ring == NULL || ring->payload_base == NULL ||
      descriptor->payload_offset < ring->superblock->payload_offset) {
    return NULL;
  }
  return (const int16_t*)((const uint8_t*)ring->mapping + descriptor->payload_offset);
}

const int16_t* mini_gnb_c_sc16_ring_map_get_channel_payload(const mini_gnb_c_sc16_ring_map_t* ring,
                                                            const uint64_t seqno,
                                                            const uint32_t channel_index) {
  const int16_t* payload = NULL;

  if (ring == NULL || ring->superblock == NULL || channel_index >= ring->superblock->channel_count) {
    return NULL;
  }
  payload = mini_gnb_c_sc16_ring_map_get_payload(ring, seqno);
  if (payload == NULL) {
    return NULL;
  }
  return payload + ((size_t)channel_index * ring->superblock->block_samples * 2u);
}

int mini_gnb_c_sc16_ring_map_append(mini_gnb_c_sc16_ring_map_t* ring,
                                    const int16_t* iq_samples,
                                    const uint32_t sample_count,
                                    const uint64_t hw_time_ns,
                                    const uint32_t flags,
                                    char* error_message,
                                    const size_t error_message_size) {
  mini_gnb_c_sc16_ring_map_superblock_t* sb = NULL;
  mini_gnb_c_sc16_ring_map_descriptor_t* descriptor = NULL;
  uint8_t* payload = NULL;
  uint64_t seqno = 0u;
  size_t slot = 0u;
  size_t payload_bytes = 0u;

  if (ring == NULL || ring->superblock == NULL || ring->descriptors == NULL || !ring->writable) {
    return mini_gnb_c_sc16_ring_map_fail(error_message, error_message_size, "ring map is not writable");
  }
  if (iq_samples == NULL || sample_count == 0u || sample_count > ring->superblock->block_samples) {
    return mini_gnb_c_sc16_ring_map_fail(error_message, error_message_size, "invalid sc16 block append request");
  }

  sb = ring->superblock;
  seqno = sb->next_write_seq;
  slot = mini_gnb_c_sc16_ring_map_slot_for_seq(ring, seqno);
  descriptor = &ring->descriptors[slot];
  payload = ring->payload_base + (slot * sb->block_bytes);
  payload_bytes = (size_t)sample_count * sizeof(int16_t) * 2u * sb->channel_count;

  descriptor->state = MINI_GNB_C_SC16_RING_BLOCK_WRITING;
  __sync_synchronize();
  memset(payload, 0, sb->block_bytes);
  {
    uint32_t channel_index = 0u;

    for (channel_index = 0u; channel_index < sb->channel_count; ++channel_index) {
      const int16_t* src = iq_samples + ((size_t)channel_index * sb->block_samples * 2u);
      uint8_t* dst = payload + ((size_t)channel_index * sb->block_samples * sizeof(int16_t) * 2u);

      memcpy(dst, src, (size_t)sample_count * sizeof(int16_t) * 2u);
    }
  }
  descriptor->seqno = seqno;
  descriptor->hw_time_ns = hw_time_ns;
  descriptor->payload_offset = sb->payload_offset + ((uint64_t)slot * sb->block_bytes);
  descriptor->sample_count = sample_count;
  descriptor->payload_bytes = (uint32_t)payload_bytes;
  descriptor->flags = flags;
  __sync_synchronize();
  descriptor->state = MINI_GNB_C_SC16_RING_BLOCK_READY;

  if (sb->next_write_seq >= sb->block_count) {
    sb->oldest_valid_seq = sb->next_write_seq - sb->block_count + 1u;
    sb->dropped_blocks += 1u;
  }
  sb->last_committed_seq = seqno;
  sb->next_write_seq = seqno + 1u;
  __sync_synchronize();
  return 0;
}
