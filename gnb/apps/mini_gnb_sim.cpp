#include <filesystem>
#include <exception>
#include <iostream>
#include <string>

#include "mini_gnb/common/simulator.hpp"
#include "mini_gnb/common/types.hpp"
#include "mini_gnb/config/config_loader.hpp"

int main(int argc, char** argv) {
  try {
    const std::string source_dir = MINI_GNB_SOURCE_DIR;
    const std::string default_config =
        (std::filesystem::path(source_dir) / "config" / "default_cell.json").string();
    const std::string config_path = argc > 1 ? argv[1] : default_config;
    const auto config = mini_gnb::load_config(config_path);

    std::cout << mini_gnb::format_config_summary(config) << "\n";

    const auto output_dir = (std::filesystem::current_path() / "out").string();
    mini_gnb::MiniGnbSimulator simulator(config, output_dir);
    const auto summary = simulator.run();

    if (summary.counters.count("rrcsetup_sent") > 0U && summary.counters.at("rrcsetup_sent") > 0U) {
      std::cout << "\nRun result: Msg1 -> Msg4 simulated successfully.\n";
    } else {
      std::cout << "\nRun result: Msg4 was not sent.\n";
    }

    std::cout << "Artifacts:\n";
    std::cout << "  - " << summary.trace_path << "\n";
    std::cout << "  - " << summary.metrics_path << "\n";
    std::cout << "  - " << summary.summary_path << "\n";
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "mini_gnb_sim failed: " << ex.what() << "\n";
    return 1;
  }
}
