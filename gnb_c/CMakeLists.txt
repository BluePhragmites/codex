cmake_minimum_required(VERSION 3.20)

project(mini_gnb_c LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(MINI_GNB_C_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(MINI_GNB_C_RADIO_FORCE_RELEASE "Build radio-facing code with release-style optimization even in Debug" ON)
option(MINI_GNB_C_RADIO_FORCE_DEBUG "Build radio-facing code with debug-style flags" OFF)

if(MINI_GNB_C_RADIO_FORCE_RELEASE AND MINI_GNB_C_RADIO_FORCE_DEBUG)
  message(FATAL_ERROR "MINI_GNB_C_RADIO_FORCE_RELEASE and MINI_GNB_C_RADIO_FORCE_DEBUG cannot both be ON")
endif()

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
find_package(Threads REQUIRED)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(UHD QUIET IMPORTED_TARGET uhd)
endif()

add_library(mini_gnb_c_lib
  src/common/types.c
  src/common/hex.c
  src/common/json_utils.c
  src/common/simulator.c
  src/core/gnb_core_bridge.c
  src/core/core_session.c
  src/config/config_loader.c
  src/link/json_link.c
  src/link/shared_slot_link.c
  src/ngap/ngap_runtime.c
  src/ngap/ngap_transport.c
  src/nas/nas_5gs_min.c
  src/rlc/rlc_lite.c
  src/timing/slot_engine.c
  src/trace/pcap_trace.c
  src/n3/gtpu_tunnel.c
  src/n3/n3_user_plane.c
  src/radio/air_pdu.c
  src/radio/b210_app_runtime.c
  src/radio/b210_slot_backend.c
  src/radio/host_performance.c
  src/radio/radio_frontend.c
  src/radio/mock_radio_frontend.c
  src/radio/sc16_ring_export.c
  src/radio/sc16_ring_map.c
  src/broadcast/broadcast_engine.c
  src/phy_ul/mock_prach_detector.c
  src/ra/ra_manager.c
  src/phy_dl/rar_builder.c
  src/phy_dl/msg4_builder.c
  src/phy_dl/mock_dl_phy_mapper.c
  src/scheduler/initial_access_scheduler.c
  src/phy_ul/mock_msg3_receiver.c
  src/ue/mini_ue_fsm.c
  src/ue/mini_ue_runtime.c
  src/ue/ue_ip_stack_min.c
  src/ue/ue_tun.c
  src/mac/mac_ul_demux.c
  src/rrc/rrc_ccch_stub.c
  src/ue/ue_context_store.c
  src/metrics/metrics_trace.c
)

set(MINI_GNB_C_RADIO_SOURCE_FILES
  src/radio/air_pdu.c
  src/radio/b210_app_runtime.c
  src/radio/b210_slot_backend.c
  src/radio/host_performance.c
  src/radio/radio_frontend.c
  src/radio/mock_radio_frontend.c
  src/radio/sc16_ring_export.c
  src/radio/sc16_ring_map.c
)

set(MINI_GNB_C_RADIO_APP_SOURCE_FILES
  apps/mini_b210_probe_c.c
  src/radio/b210_uhd_probe.c
)

if(MSVC)
  if(MINI_GNB_C_RADIO_FORCE_RELEASE)
    set(MINI_GNB_C_RADIO_PROFILE_FLAGS /O2 /DNDEBUG)
  elseif(MINI_GNB_C_RADIO_FORCE_DEBUG)
    set(MINI_GNB_C_RADIO_PROFILE_FLAGS /Od /Zi)
  endif()
else()
  if(MINI_GNB_C_RADIO_FORCE_RELEASE)
    set(MINI_GNB_C_RADIO_PROFILE_FLAGS -O3 -DNDEBUG)
  elseif(MINI_GNB_C_RADIO_FORCE_DEBUG)
    set(MINI_GNB_C_RADIO_PROFILE_FLAGS -O0 -g3 -UNDEBUG)
  endif()
endif()

if(MINI_GNB_C_RADIO_PROFILE_FLAGS)
  set_source_files_properties(${MINI_GNB_C_RADIO_SOURCE_FILES}
    PROPERTIES COMPILE_OPTIONS "${MINI_GNB_C_RADIO_PROFILE_FLAGS}")
  set_source_files_properties(${MINI_GNB_C_RADIO_APP_SOURCE_FILES}
    PROPERTIES COMPILE_OPTIONS "${MINI_GNB_C_RADIO_PROFILE_FLAGS}")
endif()

target_include_directories(mini_gnb_c_lib PUBLIC include)

if(NOT MSVC)
  target_link_libraries(mini_gnb_c_lib PUBLIC m ogscrypt)
endif()

target_compile_definitions(mini_gnb_c_lib
  PUBLIC
    MINI_GNB_C_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

if(NOT WIN32 AND UHD_FOUND)
  target_sources(mini_gnb_c_lib PRIVATE src/radio/b210_uhd_probe.c)
  target_compile_definitions(mini_gnb_c_lib PUBLIC MINI_GNB_C_HAVE_UHD=1)
  target_link_libraries(mini_gnb_c_lib PUBLIC PkgConfig::UHD Threads::Threads)
endif()

add_executable(mini_gnb_c_sim apps/mini_gnb_c_sim.c)
target_link_libraries(mini_gnb_c_sim PRIVATE mini_gnb_c_lib)

add_executable(mini_ue_c apps/mini_ue_c.c)
target_link_libraries(mini_ue_c PRIVATE mini_gnb_c_lib)

if(NOT WIN32)
  add_executable(ngap_probe apps/ngap_probe.c)
  target_compile_definitions(ngap_probe PRIVATE MINI_GNB_C_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  target_link_libraries(ngap_probe PRIVATE mini_gnb_c_lib)

  add_executable(mini_ring_inspect apps/mini_ring_inspect.c)
  target_link_libraries(mini_ring_inspect PRIVATE mini_gnb_c_lib)

  add_executable(mini_ring_export apps/mini_ring_export.c)
  target_link_libraries(mini_ring_export PRIVATE mini_gnb_c_lib)
endif()

if(NOT WIN32 AND UHD_FOUND)
  add_executable(mini_b210_probe_c
    apps/mini_b210_probe_c.c
  )
  target_link_libraries(mini_b210_probe_c PRIVATE mini_gnb_c_lib)
elseif(NOT WIN32)
  message(STATUS "UHD not found via pkg-config; skipping mini_b210_probe_c")
endif()

add_executable(mini_gnb_c_tests
  tests/test_main.c
  tests/test_config.c
  tests/test_core_session.c
  tests/test_ue_context_store.c
  tests/test_gnb_core_bridge.c
  tests/test_gtpu_tunnel.c
  tests/test_n3_user_plane.c
  tests/test_ngap_runtime.c
  tests/test_air_pdu.c
  tests/test_pcap_trace.c
  tests/test_nas_5gs_min.c
  tests/test_rlc_lite.c
  tests/test_radio_frontend.c
  tests/test_b210_probe.c
  tests/test_host_performance.c
  tests/test_sc16_ring_export.c
  tests/test_sc16_ring_map.c
  tests/test_ue_ip_stack_min.c
  tests/test_json_link.c
  tests/test_shared_slot_link.c
  tests/test_mini_ue_fsm.c
  tests/test_mini_ue_runtime.c
  tests/test_ra_manager.c
  tests/test_mac_rrc.c
  tests/test_integration.c
)
target_link_libraries(mini_gnb_c_tests PRIVATE mini_gnb_c_lib)

add_test(NAME mini_gnb_c_tests COMMAND mini_gnb_c_tests)
