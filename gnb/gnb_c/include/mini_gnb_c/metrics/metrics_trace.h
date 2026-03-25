#ifndef MINI_GNB_C_METRICS_METRICS_TRACE_H
#define MINI_GNB_C_METRICS_METRICS_TRACE_H

#include <stddef.h>
#include <stdint.h>

#include "mini_gnb_c/common/types.h"

typedef struct mini_gnb_c_metrics_trace {
  char output_dir[MINI_GNB_C_MAX_PATH];
  mini_gnb_c_counters_t counters;
  mini_gnb_c_trace_event_t events[MINI_GNB_C_MAX_EVENTS];
  size_t event_count;
  mini_gnb_c_slot_perf_t slot_perf[MINI_GNB_C_MAX_EVENTS];
  size_t slot_perf_count;
} mini_gnb_c_metrics_trace_t;

void mini_gnb_c_metrics_trace_init(mini_gnb_c_metrics_trace_t* metrics, const char* output_dir);

void mini_gnb_c_metrics_trace_increment_named(mini_gnb_c_metrics_trace_t* metrics,
                                              const char* counter_name,
                                              uint64_t value);

void mini_gnb_c_metrics_trace_event(mini_gnb_c_metrics_trace_t* metrics,
                                    const char* module,
                                    const char* message,
                                    int abs_slot,
                                    const char* details_fmt,
                                    ...);

void mini_gnb_c_metrics_trace_add_slot_perf(mini_gnb_c_metrics_trace_t* metrics,
                                            const mini_gnb_c_slot_perf_t* perf);

int mini_gnb_c_metrics_trace_flush(const mini_gnb_c_metrics_trace_t* metrics,
                                   bool has_ra_context,
                                   const mini_gnb_c_ra_context_t* ra_context,
                                   const mini_gnb_c_ue_context_t* ue_contexts,
                                   size_t ue_count,
                                   uint64_t tx_burst_count,
                                   int64_t last_hw_time_ns,
                                   mini_gnb_c_run_summary_t* out_summary);

#endif
