#pragma once

#include <string>

#include "mini_gnb/common/types.hpp"

namespace mini_gnb {

Config load_config(const std::string& path);
std::string format_config_summary(const Config& config);

}  // namespace mini_gnb
