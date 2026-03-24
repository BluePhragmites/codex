#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace mini_gnb {

std::vector<std::uint8_t> hex_to_bytes(const std::string& hex);
std::string bytes_to_hex(const std::vector<std::uint8_t>& bytes);
std::string bytes_to_hex(const std::array<std::uint8_t, 6>& bytes);

}  // namespace mini_gnb
