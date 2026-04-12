#include "mini_gnb_c/radio/host_performance.h"

#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static int mini_gnb_c_radio_host_performance_append(char* summary,
                                                    const size_t summary_size,
                                                    const char* label,
                                                    const char* value) {
  size_t len = 0u;

  if (summary == NULL || summary_size == 0u || label == NULL || value == NULL) {
    return -1;
  }
  len = strlen(summary);
  return snprintf(summary + len,
                  summary_size - len,
                  "%s%s=%s",
                  len > 0u ? " " : "",
                  label,
                  value) < (int)(summary_size - len)
             ? 0
             : -1;
}

static const char* mini_gnb_c_radio_host_performance_status(int ok) {
  return ok ? "applied" : "failed";
}

static int mini_gnb_c_radio_host_performance_write_text(const char* path, const char* text) {
  int fd = -1;
  size_t remaining = 0u;
  const char* cursor = text;

  if (path == NULL || text == NULL) {
    return -1;
  }
  fd = open(path, O_WRONLY);
  if (fd < 0) {
    return -1;
  }
  remaining = strlen(text);
  while (remaining > 0u) {
    ssize_t written = write(fd, cursor, remaining);

    if (written < 0) {
      if (errno == EINTR) {
        continue;
      }
      (void)close(fd);
      return -1;
    }
    remaining -= (size_t)written;
    cursor += written;
  }
  (void)close(fd);
  return 0;
}

static int mini_gnb_c_radio_host_performance_set_governor(void) {
  glob_t matches;
  size_t i = 0u;
  int ok = 1;

  memset(&matches, 0, sizeof(matches));
  if (glob("/sys/devices/system/cpu/cpu*/cpufreq/scaling_governor", 0, NULL, &matches) != 0) {
    return 0;
  }
  if (matches.gl_pathc == 0u) {
    globfree(&matches);
    return 0;
  }
  for (i = 0u; i < matches.gl_pathc; ++i) {
    if (mini_gnb_c_radio_host_performance_write_text(matches.gl_pathv[i], "performance\n") != 0) {
      ok = 0;
      break;
    }
  }
  globfree(&matches);
  return ok;
}

static int mini_gnb_c_radio_host_performance_disable_kms_polling(void) {
  if (access("/sys/module/drm_kms_helper/parameters/poll", F_OK) != 0) {
    return 0;
  }
  return mini_gnb_c_radio_host_performance_write_text("/sys/module/drm_kms_helper/parameters/poll", "N\n") == 0;
}

void mini_gnb_c_radio_host_performance_plan_for_backend(
    const mini_gnb_c_radio_backend_kind_t kind,
    mini_gnb_c_radio_host_performance_plan_t* plan) {
  if (plan == NULL) {
    return;
  }
  memset(plan, 0, sizeof(*plan));
  if (kind == MINI_GNB_C_RADIO_BACKEND_B210) {
    plan->apply_cpu_governor_performance = 1;
    plan->disable_drm_kms_polling = 1;
    plan->tune_network_buffers = 0;
  }
}

int mini_gnb_c_radio_host_performance_prepare_for_backend(
    const mini_gnb_c_radio_backend_kind_t kind,
    char* summary,
    const size_t summary_size) {
  mini_gnb_c_radio_host_performance_plan_t plan;

  if (summary == NULL || summary_size == 0u) {
    return -1;
  }
  summary[0] = '\0';
  mini_gnb_c_radio_host_performance_plan_for_backend(kind, &plan);

  if (!plan.apply_cpu_governor_performance && !plan.disable_drm_kms_polling && !plan.tune_network_buffers) {
    return mini_gnb_c_radio_host_performance_append(summary, summary_size, "host_tuning", "not_applicable");
  }
  if (geteuid() != 0) {
    return mini_gnb_c_radio_host_performance_append(summary, summary_size, "host_tuning", "skipped_not_root");
  }

  if (plan.apply_cpu_governor_performance &&
      mini_gnb_c_radio_host_performance_append(summary,
                                               summary_size,
                                               "cpu_governor",
                                               mini_gnb_c_radio_host_performance_status(
                                                   mini_gnb_c_radio_host_performance_set_governor())) != 0) {
    return -1;
  }
  if (plan.disable_drm_kms_polling &&
      mini_gnb_c_radio_host_performance_append(summary,
                                               summary_size,
                                               "kms_poll",
                                               mini_gnb_c_radio_host_performance_status(
                                                   mini_gnb_c_radio_host_performance_disable_kms_polling())) != 0) {
    return -1;
  }
  if (!plan.tune_network_buffers &&
      mini_gnb_c_radio_host_performance_append(summary, summary_size, "net_buffers", "not_applicable") != 0) {
    return -1;
  }
  return 0;
}
