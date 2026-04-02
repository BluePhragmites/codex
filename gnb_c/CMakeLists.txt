cmake_minimum_required(VERSION 3.20)

project(mini_gnb_c LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(MINI_GNB_C_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

if(MSVC)
  add_compile_options(/W4)
  if(MINI_GNB_C_WARNINGS_AS_ERRORS)
    add_compile_options(/WX)
  endif()
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
  if(MINI_GNB_C_WARNINGS_AS_ERRORS)
    add_compile_options(-Werror)
  endif()
endif()

enable_testing()

add_library(mini_gnb_c_lib
  src/common/types.c
  src/common/hex.c
  src/common/json_utils.c
  src/common/simulator.c
  src/core/core_session.c
  src/config/config_loader.c
  src/link/json_link.c
  src/timing/slot_engine.c
  src/n3/gtpu_tunnel.c
  src/radio/mock_radio_frontend.c
  src/broadcast/broadcast_engine.c
  src/phy_ul/mock_prach_detector.c
  src/ra/ra_manager.c
  src/phy_dl/rar_builder.c
  src/phy_dl/msg4_builder.c
  src/phy_dl/mock_dl_phy_mapper.c
  src/scheduler/initial_access_scheduler.c
  src/phy_ul/mock_msg3_receiver.c
  src/mac/mac_ul_demux.c
  src/rrc/rrc_ccch_stub.c
  src/ue/ue_context_store.c
  src/metrics/metrics_trace.c
)

target_include_directories(mini_gnb_c_lib PUBLIC include)

if(NOT MSVC)
  target_link_libraries(mini_gnb_c_lib PUBLIC m)
endif()

target_compile_definitions(mini_gnb_c_lib
  PUBLIC
    MINI_GNB_C_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_executable(mini_gnb_c_sim apps/mini_gnb_c_sim.c)
target_link_libraries(mini_gnb_c_sim PRIVATE mini_gnb_c_lib)

if(NOT WIN32)
  add_executable(ngap_probe apps/ngap_probe.c)
  target_compile_definitions(ngap_probe PRIVATE MINI_GNB_C_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  target_link_libraries(ngap_probe PRIVATE mini_gnb_c_lib ogscrypt)
endif()

add_executable(mini_gnb_c_tests
  tests/test_main.c
  tests/test_config.c
  tests/test_core_session.c
  tests/test_gtpu_tunnel.c
  tests/test_json_link.c
  tests/test_ra_manager.c
  tests/test_mac_rrc.c
  tests/test_integration.c
)
target_link_libraries(mini_gnb_c_tests PRIVATE mini_gnb_c_lib)

add_test(NAME mini_gnb_c_tests COMMAND mini_gnb_c_tests)
