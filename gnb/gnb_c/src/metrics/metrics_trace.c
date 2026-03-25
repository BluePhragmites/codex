#include "mini_gnb_c/metrics/metrics_trace.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "mini_gnb_c/common/hex.h"
#include "mini_gnb_c/common/json_utils.h"

static void mini_gnb_c_copy_string(char* out, size_t out_size, const char* text) {
  if (out == NULL || out_size == 0U) {
    return;
  }

  if (text == NULL) {
    out[0] = '\0';
    return;
  }

  (void)snprintf(out, out_size, "%s", text);
}

static int mini_gnb_c_ensure_directory_recursive(const char* path) {
  char temp[MINI_GNB_C_MAX_PATH];
  size_t i = 0;

  if (path == NULL) {
    return -1;
  }

  (void)snprintf(temp, sizeof(temp), "%s", path);
  for (i = 1; temp[i] != '\0'; ++i) {
    if (temp[i] == '/' || temp[i] == '\\') {
      char saved = temp[i];
      temp[i] = '\0';
      if (strlen(temp) > 0U && mkdir(temp, 0777) != 0 && errno != EEXIST) {
        temp[i] = saved;
        return -1;
      }
      temp[i] = saved;
    }
  }

  if (mkdir(temp, 0777) != 0 && errno != EEXIST) {
    return -1;
  }
  return 0;
}

static void mini_gnb_c_json_write_string(FILE* file, const char* text) {
  size_t i = 0;

  fputc('"', file);
  if (text != NULL) {
    for (i = 0; text[i] != '\0'; ++i) {
      switch (text[i]) {
        case '\\':
        case '"':
          fputc('\\', file);
          fputc((int)text[i], file);
          break;
        case '\n':
          fputs("\\n", file);
          break;
        case '\r':
          fputs("\\r", file);
          break;
        case '\t':
          fputs("\\t", file);
          break;
        default:
          fputc((int)text[i], file);
          break;
      }
    }
  }
  fputc('"', file);
}

static void mini_gnb_c_write_trace_json(FILE* file, const mini_gnb_c_metrics_trace_t* metrics) {
  size_t i = 0;

  fputs("[\n", file);
  for (i = 0; i < metrics->event_count; ++i) {
    const mini_gnb_c_trace_event_t* event = &metrics->events[i];
    fputs("  {\"module\":", file);
    mini_gnb_c_json_write_string(file, event->module);
    fputs(",\"message\":", file);
    mini_gnb_c_json_write_string(file, event->message);
    fprintf(file, ",\"abs_slot\":%d,\"details\":", event->abs_slot);
    mini_gnb_c_json_write_string(file, event->details);
    fputc('}', file);
    if (i + 1U != metrics->event_count) {
      fputc(',', file);
    }
    fputc('\n', file);
  }
  fputs("]\n", file);
}

static void mini_gnb_c_write_counters_json(FILE* file, const mini_gnb_c_counters_t* counters) {
  fprintf(file,
          "{\"prach_detect_ok\":%llu,\"prach_false_alarm\":%llu,\"rar_sent\":%llu,"
          "\"msg3_crc_ok\":%llu,\"msg3_crc_fail\":%llu,\"rrcsetup_sent\":%llu,"
          "\"ra_timeout\":%llu}",
          (unsigned long long)counters->prach_detect_ok,
          (unsigned long long)counters->prach_false_alarm,
          (unsigned long long)counters->rar_sent,
          (unsigned long long)counters->msg3_crc_ok,
          (unsigned long long)counters->msg3_crc_fail,
          (unsigned long long)counters->rrcsetup_sent,
          (unsigned long long)counters->ra_timeout);
}

static void mini_gnb_c_write_metrics_json(FILE* file, const mini_gnb_c_metrics_trace_t* metrics) {
  size_t i = 0;

  fputs("{\"counters\":", file);
  mini_gnb_c_write_counters_json(file, &metrics->counters);
  fputs(",\"slot_perf\":[", file);
  for (i = 0; i < metrics->slot_perf_count; ++i) {
    const mini_gnb_c_slot_perf_t* perf = &metrics->slot_perf[i];
    fprintf(file,
            "{\"abs_slot\":%d,\"mac_latency_us\":%d,\"dl_build_latency_us\":%d,"
            "\"ul_decode_latency_us\":%d}",
            perf->abs_slot,
            perf->mac_latency_us,
            perf->dl_build_latency_us,
            perf->ul_decode_latency_us);
    if (i + 1U != metrics->slot_perf_count) {
      fputc(',', file);
    }
  }
  fputs("]}\n", file);
}

static void mini_gnb_c_write_ra_context_json(FILE* file,
                                             bool has_ra_context,
                                             const mini_gnb_c_ra_context_t* ra_context) {
  char contention_id_hex[MINI_GNB_C_MAX_TEXT];

  if (!has_ra_context || ra_context == NULL) {
    fputs("null", file);
    return;
  }

  if (ra_context->has_contention_id) {
    (void)mini_gnb_c_bytes_to_hex(ra_context->contention_id48,
                                  6U,
                                  contention_id_hex,
                                  sizeof(contention_id_hex));
  } else {
    contention_id_hex[0] = '\0';
  }

  fputs("{\"detect_abs_slot\":", file);
  fprintf(file, "%d", ra_context->detect_abs_slot);
  fprintf(file,
          ",\"preamble_id\":%u,\"tc_rnti\":%u,\"ta_est\":%d,\"rar_abs_slot\":%d,"
          "\"msg3_expect_abs_slot\":%d,\"msg4_abs_slot\":%d,\"state\":",
          ra_context->preamble_id,
          ra_context->tc_rnti,
          ra_context->ta_est,
          ra_context->rar_abs_slot,
          ra_context->msg3_expect_abs_slot,
          ra_context->msg4_abs_slot);
  mini_gnb_c_json_write_string(file, mini_gnb_c_ra_state_to_string(ra_context->state));
  fprintf(file,
          ",\"msg3_harq_round\":%d,\"ue_ctx_promoted\":%s,\"has_contention_id\":%s,"
          "\"contention_id48\":",
          ra_context->msg3_harq_round,
          ra_context->ue_ctx_promoted ? "true" : "false",
          ra_context->has_contention_id ? "true" : "false");
  if (ra_context->has_contention_id) {
    mini_gnb_c_json_write_string(file, contention_id_hex);
  } else {
    fputs("null", file);
  }
  fputs(",\"last_failure\":", file);
  if (ra_context->last_failure[0] != '\0') {
    mini_gnb_c_json_write_string(file, ra_context->last_failure);
  } else {
    fputs("null", file);
  }
  fputc('}', file);
}

static void mini_gnb_c_write_ue_contexts_json(FILE* file,
                                              const mini_gnb_c_ue_context_t* ue_contexts,
                                              size_t ue_count) {
  size_t i = 0;

  fputc('[', file);
  for (i = 0; i < ue_count; ++i) {
    char contention_id_hex[MINI_GNB_C_MAX_TEXT];
    (void)mini_gnb_c_bytes_to_hex(ue_contexts[i].contention_id48,
                                  6U,
                                  contention_id_hex,
                                  sizeof(contention_id_hex));
    fprintf(file,
            "{\"tc_rnti\":%u,\"c_rnti\":%u,\"contention_id48\":",
            ue_contexts[i].tc_rnti,
            ue_contexts[i].c_rnti);
    mini_gnb_c_json_write_string(file, contention_id_hex);
    fprintf(file,
            ",\"create_abs_slot\":%d,\"rrc_setup_sent\":%s,\"sent_abs_slot\":%d}",
            ue_contexts[i].create_abs_slot,
            ue_contexts[i].rrc_setup_sent ? "true" : "false",
            ue_contexts[i].sent_abs_slot);
    if (i + 1U != ue_count) {
      fputc(',', file);
    }
  }
  fputc(']', file);
}

static int mini_gnb_c_write_summary_json(const char* path,
                                         const mini_gnb_c_metrics_trace_t* metrics,
                                         bool has_ra_context,
                                         const mini_gnb_c_ra_context_t* ra_context,
                                         const mini_gnb_c_ue_context_t* ue_contexts,
                                         size_t ue_count,
                                         uint64_t tx_burst_count,
                                         int64_t last_hw_time_ns,
                                         const char* trace_path,
                                         const char* metrics_path,
                                         const char* summary_path) {
  FILE* file = fopen(path, "wb");
  if (file == NULL) {
    return -1;
  }

  fputs("{\"counters\":", file);
  mini_gnb_c_write_counters_json(file, &metrics->counters);
  fprintf(file,
          ",\"radio\":{\"tx_burst_count\":%llu,\"last_hw_time_ns\":%lld},\"ra_context\":",
          (unsigned long long)tx_burst_count,
          (long long)last_hw_time_ns);
  mini_gnb_c_write_ra_context_json(file, has_ra_context, ra_context);
  fputs(",\"ue_contexts\":", file);
  mini_gnb_c_write_ue_contexts_json(file, ue_contexts, ue_count);
  fputs(",\"trace_path\":", file);
  mini_gnb_c_json_write_string(file, trace_path);
  fputs(",\"metrics_path\":", file);
  mini_gnb_c_json_write_string(file, metrics_path);
  fputs(",\"summary_path\":", file);
  mini_gnb_c_json_write_string(file, summary_path);
  fputs("}\n", file);

  fclose(file);
  return 0;
}

void mini_gnb_c_metrics_trace_init(mini_gnb_c_metrics_trace_t* metrics, const char* output_dir) {
  if (metrics == NULL) {
    return;
  }

  memset(metrics, 0, sizeof(*metrics));
  mini_gnb_c_copy_string(metrics->output_dir, sizeof(metrics->output_dir), output_dir);
}

void mini_gnb_c_metrics_trace_increment_named(mini_gnb_c_metrics_trace_t* metrics,
                                              const char* counter_name,
                                              uint64_t value) {
  if (metrics == NULL || counter_name == NULL) {
    return;
  }

  if (strcmp(counter_name, "prach_detect_ok") == 0) {
    metrics->counters.prach_detect_ok += value;
  } else if (strcmp(counter_name, "prach_false_alarm") == 0) {
    metrics->counters.prach_false_alarm += value;
  } else if (strcmp(counter_name, "rar_sent") == 0) {
    metrics->counters.rar_sent += value;
  } else if (strcmp(counter_name, "msg3_crc_ok") == 0) {
    metrics->counters.msg3_crc_ok += value;
  } else if (strcmp(counter_name, "msg3_crc_fail") == 0) {
    metrics->counters.msg3_crc_fail += value;
  } else if (strcmp(counter_name, "rrcsetup_sent") == 0) {
    metrics->counters.rrcsetup_sent += value;
  } else if (strcmp(counter_name, "ra_timeout") == 0) {
    metrics->counters.ra_timeout += value;
  }
}

void mini_gnb_c_metrics_trace_event(mini_gnb_c_metrics_trace_t* metrics,
                                    const char* module,
                                    const char* message,
                                    int abs_slot,
                                    const char* details_fmt,
                                    ...) {
  mini_gnb_c_trace_event_t* event = NULL;
  va_list args;

  if (metrics == NULL) {
    return;
  }

  if (metrics->event_count == MINI_GNB_C_MAX_EVENTS) {
    memmove(&metrics->events[0],
            &metrics->events[1],
            sizeof(metrics->events[0]) * (MINI_GNB_C_MAX_EVENTS - 1U));
    metrics->event_count = MINI_GNB_C_MAX_EVENTS - 1U;
  }

  event = &metrics->events[metrics->event_count++];
  memset(event, 0, sizeof(*event));
  mini_gnb_c_copy_string(event->module, sizeof(event->module), module);
  mini_gnb_c_copy_string(event->message, sizeof(event->message), message);
  event->abs_slot = abs_slot;

  if (details_fmt != NULL && details_fmt[0] != '\0') {
    va_start(args, details_fmt);
    (void)vsnprintf(event->details, sizeof(event->details), details_fmt, args);
    va_end(args);
  }

  printf("[%s] %s", event->module, event->message);
  if (abs_slot >= 0) {
    printf(" abs_slot=%d", abs_slot);
  }
  if (event->details[0] != '\0') {
    printf(" %s", event->details);
  }
  printf("\n");
}

void mini_gnb_c_metrics_trace_add_slot_perf(mini_gnb_c_metrics_trace_t* metrics,
                                            const mini_gnb_c_slot_perf_t* perf) {
  if (metrics == NULL || perf == NULL) {
    return;
  }

  if (metrics->slot_perf_count == MINI_GNB_C_MAX_EVENTS) {
    memmove(&metrics->slot_perf[0],
            &metrics->slot_perf[1],
            sizeof(metrics->slot_perf[0]) * (MINI_GNB_C_MAX_EVENTS - 1U));
    metrics->slot_perf_count = MINI_GNB_C_MAX_EVENTS - 1U;
  }

  metrics->slot_perf[metrics->slot_perf_count++] = *perf;
}

int mini_gnb_c_metrics_trace_flush(const mini_gnb_c_metrics_trace_t* metrics,
                                   bool has_ra_context,
                                   const mini_gnb_c_ra_context_t* ra_context,
                                   const mini_gnb_c_ue_context_t* ue_contexts,
                                   size_t ue_count,
                                   uint64_t tx_burst_count,
                                   int64_t last_hw_time_ns,
                                   mini_gnb_c_run_summary_t* out_summary) {
  char trace_path[MINI_GNB_C_MAX_PATH];
  char metrics_path[MINI_GNB_C_MAX_PATH];
  char summary_path[MINI_GNB_C_MAX_PATH];
  FILE* file = NULL;
  size_t bounded_ue_count = ue_count;

  if (metrics == NULL || out_summary == NULL) {
    return -1;
  }

  if (bounded_ue_count > MINI_GNB_C_MAX_UES) {
    bounded_ue_count = MINI_GNB_C_MAX_UES;
  }

  if (mini_gnb_c_ensure_directory_recursive(metrics->output_dir) != 0) {
    return -1;
  }

  if (mini_gnb_c_join_path(metrics->output_dir, "trace.json", trace_path, sizeof(trace_path)) != 0 ||
      mini_gnb_c_join_path(metrics->output_dir, "metrics.json", metrics_path, sizeof(metrics_path)) != 0 ||
      mini_gnb_c_join_path(metrics->output_dir, "summary.json", summary_path, sizeof(summary_path)) != 0) {
    return -1;
  }

  file = fopen(trace_path, "wb");
  if (file == NULL) {
    return -1;
  }
  mini_gnb_c_write_trace_json(file, metrics);
  fclose(file);

  file = fopen(metrics_path, "wb");
  if (file == NULL) {
    return -1;
  }
  mini_gnb_c_write_metrics_json(file, metrics);
  fclose(file);

  if (mini_gnb_c_write_summary_json(summary_path,
                                    metrics,
                                    has_ra_context,
                                    ra_context,
                                    ue_contexts,
                                    bounded_ue_count,
                                    tx_burst_count,
                                    last_hw_time_ns,
                                    trace_path,
                                    metrics_path,
                                    summary_path) != 0) {
    return -1;
  }

  memset(out_summary, 0, sizeof(*out_summary));
  out_summary->counters = metrics->counters;
  out_summary->has_ra_context = has_ra_context;
  if (has_ra_context && ra_context != NULL) {
    out_summary->ra_context = *ra_context;
  }
  if (ue_contexts != NULL && bounded_ue_count > 0U) {
    memcpy(out_summary->ue_contexts,
           ue_contexts,
           sizeof(out_summary->ue_contexts[0]) * bounded_ue_count);
  }
  out_summary->ue_count = bounded_ue_count;
  mini_gnb_c_copy_string(out_summary->trace_path, sizeof(out_summary->trace_path), trace_path);
  mini_gnb_c_copy_string(out_summary->metrics_path, sizeof(out_summary->metrics_path), metrics_path);
  mini_gnb_c_copy_string(out_summary->summary_path, sizeof(out_summary->summary_path), summary_path);
  return 0;
}
