cmake_minimum_required(VERSION 3.20)

project(mini_gnb LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(MINI_GNB_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

if(MSVC)
  add_compile_options(/W4 /permissive-)
  if(MINI_GNB_WARNINGS_AS_ERRORS)
    add_compile_options(/WX)
  endif()
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
  if(MINI_GNB_WARNINGS_AS_ERRORS)
    add_compile_options(-Werror)
  endif()
endif()

enable_testing()

add_library(mini_gnb_lib
  src/common/hex.cpp
  src/common/json_utils.cpp
  src/common/simulator.cpp
  src/config/config_loader.cpp
  src/timing/slot_engine.cpp
  src/radio/mock_radio_frontend.cpp
  src/broadcast/pbch_ssb_builder.cpp
  src/broadcast/sib1_builder.cpp
  src/broadcast/broadcast_engine.cpp
  src/phy_ul/mock_prach_detector.cpp
  src/ra/ra_manager.cpp
  src/phy_dl/rar_builder.cpp
  src/scheduler/initial_access_scheduler.cpp
  src/phy_ul/mock_msg3_receiver.cpp
  src/mac/mac_ul_demux.cpp
  src/rrc/rrc_ccch_stub.cpp
  src/phy_dl/msg4_builder.cpp
  src/ue/ue_context_store.cpp
  src/phy_dl/mock_dl_phy_mapper.cpp
  src/metrics/metrics_trace.cpp
)

target_include_directories(mini_gnb_lib PUBLIC include)

target_compile_definitions(mini_gnb_lib
  PUBLIC
    MINI_GNB_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_executable(mini_gnb_sim apps/mini_gnb_sim.cpp)
target_link_libraries(mini_gnb_sim PRIVATE mini_gnb_lib)

add_executable(mini_gnb_tests
  tests/test_main.cpp
  tests/test_config.cpp
  tests/test_ra_manager.cpp
  tests/test_mac_rrc.cpp
  tests/test_integration.cpp
)
target_link_libraries(mini_gnb_tests PRIVATE mini_gnb_lib)

add_test(NAME mini_gnb_tests COMMAND mini_gnb_tests)
