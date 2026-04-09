#define _POSIX_C_SOURCE 200809L

#include "mini_gnb_c/link/shared_slot_link.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MINI_GNB_C_SHARED_SLOT_MAGIC 0x4d474c53u
#define MINI_GNB_C_SHARED_SLOT_VERSION 2u

typedef struct {
  uint32_t magic;
  uint32_t version;
  volatile int32_t gnb_tx_slot;
  volatile int32_t ue_rx_slot;
  volatile uint32_t gnb_done;
  volatile uint32_t ue_done;
  volatile uint32_t ul_event_count;
  mini_gnb_c_shared_slot_dl_summary_t dl_summary;
  mini_gnb_c_shared_slot_ul_event_t ul_events[MINI_GNB_C_SHARED_SLOT_MAX_UL_EVENTS];
} mini_gnb_c_shared_slot_region_t;

static void mini_gnb_c_memory_barrier(void) {
  __sync_synchronize();
}

static void mini_gnb_c_sleep_1ms(void) {
  struct timespec ts;
  ts.tv_sec = 0;
  ts.tv_nsec = 1000000L;
  (void)nanosleep(&ts, NULL);
}

static int mini_gnb_c_get_monotonic_ms(uint64_t* out_ms) {
  struct timespec ts;

  if (out_ms == NULL) {
    return -1;
  }
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return -1;
  }
  *out_ms = (uint64_t)ts.tv_sec * 1000u + (uint64_t)(ts.tv_nsec / 1000000u);
  return 0;
}

static int mini_gnb_c_shared_slot_ensure_parent_dir(const char* path) {
  char temp[MINI_GNB_C_MAX_PATH];
  size_t i = 0u;

  if (path == NULL || path[0] == '\0') {
    return -1;
  }

  if (snprintf(temp, sizeof(temp), "%s", path) >= (int)sizeof(temp)) {
    return -1;
  }

  for (i = 1u; temp[i] != '\0'; ++i) {
    if (temp[i] == '/' || temp[i] == '\\') {
      char saved = temp[i];
      temp[i] = '\0';
      if (strlen(temp) > 0u && mkdir(temp, 0777) != 0 && errno != EEXIST) {
        temp[i] = saved;
        return -1;
      }
      temp[i] = saved;
    }
  }

  return 0;
}

static mini_gnb_c_shared_slot_region_t* mini_gnb_c_shared_slot_region(mini_gnb_c_shared_slot_link_t* link) {
  if (link == NULL || link->mapping == NULL) {
    return NULL;
  }
  return (mini_gnb_c_shared_slot_region_t*)link->mapping;
}

static const mini_gnb_c_shared_slot_region_t* mini_gnb_c_shared_slot_region_const(
    const mini_gnb_c_shared_slot_link_t* link) {
  if (link == NULL || link->mapping == NULL) {
    return NULL;
  }
  return (const mini_gnb_c_shared_slot_region_t*)link->mapping;
}

static void mini_gnb_c_shared_slot_reset_region(mini_gnb_c_shared_slot_region_t* region) {
  if (region == NULL) {
    return;
  }

  memset(region, 0, sizeof(*region));
  region->magic = MINI_GNB_C_SHARED_SLOT_MAGIC;
  region->version = MINI_GNB_C_SHARED_SLOT_VERSION;
  region->gnb_tx_slot = -1;
  region->ue_rx_slot = -1;
  region->dl_summary.abs_slot = -1;
  region->ul_event_count = 0u;
}

int mini_gnb_c_shared_slot_link_open(mini_gnb_c_shared_slot_link_t* link, const char* path, const bool reset_region) {
  mini_gnb_c_shared_slot_region_t* region = NULL;

  if (link == NULL || path == NULL || path[0] == '\0') {
    return -1;
  }

  memset(link, 0, sizeof(*link));
  link->fd = -1;
  link->mapping_size = sizeof(mini_gnb_c_shared_slot_region_t);

  if (snprintf(link->path, sizeof(link->path), "%s", path) >= (int)sizeof(link->path)) {
    return -1;
  }
  if (mini_gnb_c_shared_slot_ensure_parent_dir(path) != 0) {
    return -1;
  }

  link->fd = open(path, O_RDWR | O_CREAT, 0666);
  if (link->fd < 0) {
    return -1;
  }
  if (ftruncate(link->fd, (off_t)link->mapping_size) != 0) {
    close(link->fd);
    link->fd = -1;
    return -1;
  }

  link->mapping = mmap(NULL, link->mapping_size, PROT_READ | PROT_WRITE, MAP_SHARED, link->fd, 0);
  if (link->mapping == MAP_FAILED) {
    close(link->fd);
    link->fd = -1;
    link->mapping = NULL;
    return -1;
  }

  region = mini_gnb_c_shared_slot_region(link);
  if (reset_region || region->magic != MINI_GNB_C_SHARED_SLOT_MAGIC || region->version != MINI_GNB_C_SHARED_SLOT_VERSION) {
    mini_gnb_c_shared_slot_reset_region(region);
    mini_gnb_c_memory_barrier();
  }

  return 0;
}

void mini_gnb_c_shared_slot_link_close(mini_gnb_c_shared_slot_link_t* link) {
  if (link == NULL) {
    return;
  }

  if (link->mapping != NULL) {
    (void)munmap(link->mapping, link->mapping_size);
  }
  if (link->fd >= 0) {
    (void)close(link->fd);
  }
  memset(link, 0, sizeof(*link));
  link->fd = -1;
}

int mini_gnb_c_shared_slot_link_publish_gnb_slot(mini_gnb_c_shared_slot_link_t* link,
                                                 const mini_gnb_c_shared_slot_dl_summary_t* summary) {
  mini_gnb_c_shared_slot_region_t* region = mini_gnb_c_shared_slot_region(link);

  if (region == NULL || summary == NULL) {
    return -1;
  }

  region->dl_summary = *summary;
  mini_gnb_c_memory_barrier();
  region->gnb_tx_slot = summary->abs_slot;
  mini_gnb_c_memory_barrier();
  return 0;
}

int mini_gnb_c_shared_slot_link_wait_for_gnb_slot(mini_gnb_c_shared_slot_link_t* link,
                                                  const int target_abs_slot,
                                                  const uint32_t timeout_ms,
                                                  mini_gnb_c_shared_slot_dl_summary_t* out_summary) {
  mini_gnb_c_shared_slot_region_t* region = mini_gnb_c_shared_slot_region(link);
  uint64_t start_ms = 0u;

  if (region == NULL || target_abs_slot < 0) {
    return -1;
  }
  if (mini_gnb_c_get_monotonic_ms(&start_ms) != 0) {
    return -1;
  }

  for (;;) {
    uint64_t now_ms = start_ms;
    const int published_slot = region->gnb_tx_slot;

    mini_gnb_c_memory_barrier();
    if (published_slot >= target_abs_slot) {
      if (out_summary != NULL) {
        *out_summary = region->dl_summary;
      }
      return 0;
    }
    if (region->gnb_done != 0u) {
      return 1;
    }
    if (mini_gnb_c_get_monotonic_ms(&now_ms) != 0) {
      return -1;
    }
    if (timeout_ms != 0u && now_ms - start_ms >= timeout_ms) {
      return 1;
    }
    mini_gnb_c_sleep_1ms();
  }
}

int mini_gnb_c_shared_slot_link_mark_ue_progress(mini_gnb_c_shared_slot_link_t* link, const int abs_slot) {
  mini_gnb_c_shared_slot_region_t* region = mini_gnb_c_shared_slot_region(link);

  if (region == NULL) {
    return -1;
  }

  mini_gnb_c_memory_barrier();
  region->ue_rx_slot = abs_slot;
  mini_gnb_c_memory_barrier();
  return 0;
}

int mini_gnb_c_shared_slot_link_wait_for_ue_progress(mini_gnb_c_shared_slot_link_t* link,
                                                     const int target_abs_slot,
                                                     const uint32_t timeout_ms) {
  mini_gnb_c_shared_slot_region_t* region = mini_gnb_c_shared_slot_region(link);
  uint64_t start_ms = 0u;

  if (region == NULL || target_abs_slot < 0) {
    return -1;
  }
  if (mini_gnb_c_get_monotonic_ms(&start_ms) != 0) {
    return -1;
  }

  for (;;) {
    uint64_t now_ms = start_ms;
    const int consumed_slot = region->ue_rx_slot;

    mini_gnb_c_memory_barrier();
    if (consumed_slot >= target_abs_slot) {
      return 0;
    }
    if (region->ue_done != 0u) {
      return 1;
    }
    if (mini_gnb_c_get_monotonic_ms(&now_ms) != 0) {
      return -1;
    }
    if (timeout_ms != 0u && now_ms - start_ms >= timeout_ms) {
      return 1;
    }
    mini_gnb_c_sleep_1ms();
  }
}

int mini_gnb_c_shared_slot_link_schedule_ul(mini_gnb_c_shared_slot_link_t* link,
                                            const mini_gnb_c_shared_slot_ul_event_t* event) {
  mini_gnb_c_shared_slot_region_t* region = mini_gnb_c_shared_slot_region(link);

  if (region == NULL || event == NULL || !event->valid || event->abs_slot < 0) {
    return -1;
  }
  if (region->ul_event_count >= MINI_GNB_C_SHARED_SLOT_MAX_UL_EVENTS) {
    return 1;
  }

  region->ul_events[region->ul_event_count] = *event;
  mini_gnb_c_memory_barrier();
  ++region->ul_event_count;
  mini_gnb_c_memory_barrier();
  return 0;
}

int mini_gnb_c_shared_slot_link_consume_ul(mini_gnb_c_shared_slot_link_t* link,
                                           const int abs_slot,
                                           mini_gnb_c_shared_slot_ul_event_t* out_event) {
  mini_gnb_c_shared_slot_region_t* region = mini_gnb_c_shared_slot_region(link);
  uint32_t read_index = 0u;
  uint32_t write_index = 0u;
  bool found_match = false;
  mini_gnb_c_shared_slot_ul_event_t matched_event;

  if (region == NULL || abs_slot < 0) {
    return -1;
  }

  mini_gnb_c_memory_barrier();
  if (region->ul_event_count == 0u) {
    return 0;
  }
  memset(&matched_event, 0, sizeof(matched_event));
  for (read_index = 0u; read_index < region->ul_event_count; ++read_index) {
    const mini_gnb_c_shared_slot_ul_event_t* event = &region->ul_events[read_index];

    if (!event->valid || event->abs_slot < abs_slot) {
      continue;
    }
    if (!found_match && event->abs_slot == abs_slot) {
      matched_event = *event;
      found_match = true;
      continue;
    }
    region->ul_events[write_index++] = *event;
  }
  if (!found_match) {
    region->ul_event_count = write_index;
    mini_gnb_c_memory_barrier();
    return 0;
  }

  region->ul_event_count = write_index;
  mini_gnb_c_memory_barrier();
  if (out_event != NULL) {
    *out_event = matched_event;
  }
  return 1;
}

int mini_gnb_c_shared_slot_link_mark_gnb_done(mini_gnb_c_shared_slot_link_t* link) {
  mini_gnb_c_shared_slot_region_t* region = mini_gnb_c_shared_slot_region(link);

  if (region == NULL) {
    return -1;
  }
  mini_gnb_c_memory_barrier();
  region->gnb_done = 1u;
  mini_gnb_c_memory_barrier();
  return 0;
}

int mini_gnb_c_shared_slot_link_mark_ue_done(mini_gnb_c_shared_slot_link_t* link) {
  mini_gnb_c_shared_slot_region_t* region = mini_gnb_c_shared_slot_region(link);

  if (region == NULL) {
    return -1;
  }
  mini_gnb_c_memory_barrier();
  region->ue_done = 1u;
  mini_gnb_c_memory_barrier();
  return 0;
}

bool mini_gnb_c_shared_slot_link_gnb_done(const mini_gnb_c_shared_slot_link_t* link) {
  const mini_gnb_c_shared_slot_region_t* region = mini_gnb_c_shared_slot_region_const(link);

  return region != NULL && region->gnb_done != 0u;
}
