#pragma once

#include <stdexcept>
#include <string>

inline void require(const bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

inline std::string project_source_dir() {
  return MINI_GNB_SOURCE_DIR;
}

inline std::string default_config_path() {
  return project_source_dir() + "/config/default_cell.json";
}
