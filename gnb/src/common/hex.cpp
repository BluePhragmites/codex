#include "mini_gnb/common/hex.hpp"

#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace mini_gnb {

std::vector<std::uint8_t> hex_to_bytes(const std::string& hex) {
  std::string clean;
  clean.reserve(hex.size());
  for (const char ch : hex) {
    if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) {
      clean.push_back(ch);
    }
  }

  if ((clean.size() % 2U) != 0U) {
    throw std::runtime_error("hex string must have an even number of digits");
  }

  std::vector<std::uint8_t> bytes;
  bytes.reserve(clean.size() / 2U);
  for (std::size_t i = 0; i < clean.size(); i += 2U) {
    const auto byte = static_cast<std::uint8_t>(std::stoul(clean.substr(i, 2U), nullptr, 16));
    bytes.push_back(byte);
  }

  return bytes;
}

std::string bytes_to_hex(const std::vector<std::uint8_t>& bytes) {
  std::ostringstream stream;
  stream << std::uppercase << std::hex << std::setfill('0');
  for (const auto byte : bytes) {
    stream << std::setw(2) << static_cast<int>(byte);
  }
  return stream.str();
}

std::string bytes_to_hex(const std::array<std::uint8_t, 6>& bytes) {
  return bytes_to_hex(std::vector<std::uint8_t>(bytes.begin(), bytes.end()));
}

}  // namespace mini_gnb
