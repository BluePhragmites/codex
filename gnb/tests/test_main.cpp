#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

void test_config_loads();
void test_ra_manager_flow();
void test_ra_timeout();
void test_mac_rrc_and_msg4_contention_identity();
void test_integration_run();

int main() {
  const std::vector<std::pair<std::string, std::function<void()>>> tests = {
      {"test_config_loads", test_config_loads},
      {"test_ra_manager_flow", test_ra_manager_flow},
      {"test_ra_timeout", test_ra_timeout},
      {"test_mac_rrc_and_msg4_contention_identity", test_mac_rrc_and_msg4_contention_identity},
      {"test_integration_run", test_integration_run},
  };

  try {
    for (const auto& [name, fn] : tests) {
      fn();
      std::cout << "[PASS] " << name << "\n";
    }
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "[FAIL] " << ex.what() << "\n";
    return 1;
  }
}
