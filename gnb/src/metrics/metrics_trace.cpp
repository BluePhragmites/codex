#include "mini_gnb/metrics/metrics_trace.hpp"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <utility>

#include "mini_gnb/common/hex.hpp"
#include "mini_gnb/common/json_utils.hpp"

namespace mini_gnb {

namespace {

std::string serialize_ra_context(const std::optional<RaContext>& ra_context) {
  if (!ra_context.has_value()) {
    return "null";
  }

  const auto& ra = ra_context.value();
  return json_object({
      {"detect_abs_slot", std::to_string(ra.detect_abs_slot)},
      {"preamble_id", std::to_string(ra.preamble_id)},
      {"tc_rnti", std::to_string(ra.tc_rnti)},
      {"ta_est", std::to_string(ra.ta_est)},
      {"rar_abs_slot", std::to_string(ra.rar_abs_slot)},
      {"msg3_expect_abs_slot", std::to_string(ra.msg3_expect_abs_slot)},
      {"msg4_abs_slot", std::to_string(ra.msg4_abs_slot)},
      {"state", json_quote(to_string(ra.state))},
      {"msg3_harq_round", std::to_string(ra.msg3_harq_round)},
      {"ue_ctx_promoted", ra.ue_ctx_promoted ? "true" : "false"},
      {"has_contention_id", ra.has_contention_id ? "true" : "false"},
      {"contention_id48", ra.has_contention_id ? json_quote(bytes_to_hex(ra.contention_id48)) : "null"},
      {"last_failure", ra.last_failure.empty() ? "null" : json_quote(ra.last_failure)},
  });
}

std::string serialize_ue_contexts(const std::vector<MiniUeContext>& ue_contexts) {
  std::vector<std::string> items;
  items.reserve(ue_contexts.size());
  for (const auto& ue : ue_contexts) {
    items.push_back(json_object({
        {"tc_rnti", std::to_string(ue.tc_rnti)},
        {"c_rnti", std::to_string(ue.c_rnti)},
        {"contention_id48", json_quote(bytes_to_hex(ue.contention_id48))},
        {"create_abs_slot", std::to_string(ue.create_abs_slot)},
        {"rrc_setup_sent", ue.rrc_setup_sent ? "true" : "false"},
        {"sent_abs_slot", std::to_string(ue.sent_abs_slot)},
    }));
  }
  return json_array(items);
}

}  // namespace

MetricsTrace::MetricsTrace(std::string output_dir) : output_dir_(std::move(output_dir)) {
  counters_ = {
      {"prach_detect_ok", 0},
      {"prach_false_alarm", 0},
      {"rar_sent", 0},
      {"msg3_crc_ok", 0},
      {"msg3_crc_fail", 0},
      {"rrcsetup_sent", 0},
      {"ra_timeout", 0},
  };
}

void MetricsTrace::increment(const std::string& name, const std::uint64_t value) {
  counters_[name] += value;
}

void MetricsTrace::trace(const std::string& module,
                         const std::string& message,
                         const int abs_slot,
                         std::map<std::string, std::string> data) {
  events_.push_back(TraceEvent{module, message, abs_slot, std::move(data)});
  if (events_.size() > 128U) {
    events_.erase(events_.begin());
  }

  std::cout << "[" << module << "] " << message;
  if (abs_slot >= 0) {
    std::cout << " abs_slot=" << abs_slot;
  }
  if (!events_.back().data.empty()) {
    std::cout << " ";
    bool first = true;
    for (const auto& [key, value] : events_.back().data) {
      if (!first) {
        std::cout << ",";
      }
      first = false;
      std::cout << key << "=" << value;
    }
  }
  std::cout << "\n";
}

void MetricsTrace::add_slot_perf(const SlotPerf& perf) {
  slot_perf_.push_back(perf);
}

const std::map<std::string, std::uint64_t>& MetricsTrace::counters() const {
  return counters_;
}

RunSummary MetricsTrace::flush(const std::optional<RaContext>& ra_context,
                               const std::vector<MiniUeContext>& ue_contexts,
                               const std::uint64_t tx_burst_count,
                               const std::int64_t last_hw_time_ns) const {
  std::filesystem::create_directories(output_dir_);

  std::vector<std::string> event_items;
  event_items.reserve(events_.size());
  for (const auto& event : events_) {
    std::map<std::string, std::string> fields = {
        {"module", json_quote(event.module)},
        {"message", json_quote(event.message)},
        {"abs_slot", std::to_string(event.abs_slot)},
        {"data", json_object([&]() {
           std::map<std::string, std::string> quoted;
           for (const auto& [key, value] : event.data) {
             quoted[key] = json_quote(value);
           }
           return quoted;
         }())},
    };
    event_items.push_back(json_object(fields));
  }

  std::vector<std::string> perf_items;
  perf_items.reserve(slot_perf_.size());
  for (const auto& perf : slot_perf_) {
    perf_items.push_back(json_object({
        {"abs_slot", std::to_string(perf.abs_slot)},
        {"mac_latency_us", std::to_string(perf.mac_latency_us)},
        {"dl_build_latency_us", std::to_string(perf.dl_build_latency_us)},
        {"ul_decode_latency_us", std::to_string(perf.ul_decode_latency_us)},
    }));
  }

  std::map<std::string, std::string> counter_fields;
  for (const auto& [key, value] : counters_) {
    counter_fields[key] = std::to_string(value);
  }

  const auto trace_path = join_path(output_dir_, "trace.json");
  const auto metrics_path = join_path(output_dir_, "metrics.json");
  const auto summary_path = join_path(output_dir_, "summary.json");

  write_text_file(trace_path, json_array(event_items));
  write_text_file(metrics_path,
                  json_object({
                      {"counters", json_object(counter_fields)},
                      {"slot_perf", json_array(perf_items)},
                  }));
  write_text_file(summary_path,
                  json_object({
                      {"counters", json_object(counter_fields)},
                      {"radio", json_object({
                           {"tx_burst_count", std::to_string(tx_burst_count)},
                           {"last_hw_time_ns", std::to_string(last_hw_time_ns)},
                       })},
                      {"ra_context", serialize_ra_context(ra_context)},
                      {"ue_contexts", serialize_ue_contexts(ue_contexts)},
                      {"trace_path", json_quote(trace_path)},
                      {"metrics_path", json_quote(metrics_path)},
                      {"summary_path", json_quote(summary_path)},
                  }));

  return RunSummary{
      counters_,
      ra_context,
      ue_contexts,
      trace_path,
      metrics_path,
      summary_path,
  };
}

}  // namespace mini_gnb
