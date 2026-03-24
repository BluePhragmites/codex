#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "mini_gnb/common/types.hpp"

namespace mini_gnb {

class MetricsTrace {
 public:
  explicit MetricsTrace(std::string output_dir);

  void increment(const std::string& name, std::uint64_t value = 1);
  void trace(const std::string& module,
             const std::string& message,
             int abs_slot = -1,
             std::map<std::string, std::string> data = {});
  void add_slot_perf(const SlotPerf& perf);

  const std::map<std::string, std::uint64_t>& counters() const;
  const std::string& output_dir() const;
  RunSummary flush(const std::optional<RaContext>& ra_context,
                   const std::vector<MiniUeContext>& ue_contexts,
                   std::uint64_t tx_burst_count,
                   std::int64_t last_hw_time_ns) const;

 private:
  std::string output_dir_;
  std::map<std::string, std::uint64_t> counters_;
  std::vector<TraceEvent> events_;
  std::vector<SlotPerf> slot_perf_;
};

}  // namespace mini_gnb
