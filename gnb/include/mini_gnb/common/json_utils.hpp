#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace mini_gnb {

std::string json_escape(const std::string& value);
std::string json_quote(const std::string& value);
std::string json_object(const std::map<std::string, std::string>& fields);
std::string json_array(const std::vector<std::string>& items);
std::string read_text_file(const std::string& path);
void write_text_file(const std::string& path, const std::string& content);
std::string join_path(const std::string& left, const std::string& right);

}  // namespace mini_gnb
