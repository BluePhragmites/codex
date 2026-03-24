#include "mini_gnb/common/json_utils.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace mini_gnb {

std::string json_escape(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (const char ch : value) {
    switch (ch) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped.push_back(ch);
        break;
    }
  }
  return escaped;
}

std::string json_quote(const std::string& value) {
  return "\"" + json_escape(value) + "\"";
}

std::string json_object(const std::map<std::string, std::string>& fields) {
  std::ostringstream stream;
  stream << "{";
  bool first = true;
  for (const auto& [key, value] : fields) {
    if (!first) {
      stream << ",";
    }
    first = false;
    stream << json_quote(key) << ":" << value;
  }
  stream << "}";
  return stream.str();
}

std::string json_array(const std::vector<std::string>& items) {
  std::ostringstream stream;
  stream << "[";
  for (std::size_t i = 0; i < items.size(); ++i) {
    if (i != 0U) {
      stream << ",";
    }
    stream << items[i];
  }
  stream << "]";
  return stream.str();
}

std::string read_text_file(const std::string& path) {
  std::ifstream input(path);
  if (!input.is_open()) {
    throw std::runtime_error("failed to open file: " + path);
  }
  std::ostringstream stream;
  stream << input.rdbuf();
  return stream.str();
}

void write_text_file(const std::string& path, const std::string& content) {
  const std::filesystem::path file_path(path);
  const auto parent = file_path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent);
  }

  std::ofstream output(path);
  if (!output.is_open()) {
    throw std::runtime_error("failed to write file: " + path);
  }
  output << content;
}

std::string join_path(const std::string& left, const std::string& right) {
  return (std::filesystem::path(left) / right).string();
}

}  // namespace mini_gnb
