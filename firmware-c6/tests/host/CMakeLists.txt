# =============================================================================
# firmware-c6 — HOST unit tests
# =============================================================================
# These tests build with the host's native gcc and run as plain executables.
# No ESP-IDF, no hardware. They cover portable logic (state model, control
# math, protocol framing, ...) — anything that does NOT touch real peripherals.
#
# Hardware-dependent code is kept behind the HAL (driver/* lives only in the
# HAL component). In host tests you link the module under test against *mock*
# HAL implementations instead of the real driver-backed ones, so the logic can
# run on a PC. (Mocks arrive with the HAL component; none are needed yet.)
#
# -----------------------------------------------------------------------------
# How to run
# -----------------------------------------------------------------------------
#   From the repo root (preferred — uses the shared helper):
#       scripts/test-host.sh firmware-c6
#
#   Manually, equivalent steps:
#       cmake -DTARGET_PLATFORM=host -B build/host -G Ninja tests/host
#       cmake --build build/host
#       ctest --test-dir build/host --output-on-failure
#
# -----------------------------------------------------------------------------
# How to add a new test
# -----------------------------------------------------------------------------
#   1. Create tests/host/test_<thing>.c with a main() that runs Unity (see
#      test_dummy.c for the minimal shape).
#   2. Register it at the bottom of this file:
#          add_host_test(test_<thing>
#              test_<thing>.c
#              ../../components/<module>/<module>.c)   # source(s) under test
#   3. If the source needs headers, add their dirs:
#          target_include_directories(test_<thing> PRIVATE
#              ../../components/<module>/include)
#   That's it — ctest discovers it automatically.
# =============================================================================
cmake_minimum_required(VERSION 3.16)

# This tree is host-only. TARGET_PLATFORM selects host vs. target builds across
# the repo; default it to host and refuse anything else so a stray cross/IDF
# invocation fails loudly instead of producing a broken build.
if(NOT DEFINED TARGET_PLATFORM)
    set(TARGET_PLATFORM "host")
endif()
if(NOT TARGET_PLATFORM STREQUAL "host")
    message(FATAL_ERROR
        "tests/host builds only with -DTARGET_PLATFORM=host (got '${TARGET_PLATFORM}')")
endif()

project(firmware_c6_host_tests C)

enable_testing()   # turn on ctest support

# config/thermostat_config.h is the single source of compile-time constants;
# data_model.h (and others) include it. Make it visible to every host test.
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../config)

# --- Unity test framework -----------------------------------------------------
# Vendored under unity/ (see unity/README.md) so host tests have zero ESP-IDF
# dependency. Built once as a static lib and reused by every test executable.
add_library(unity STATIC unity/unity.c)
target_include_directories(unity PUBLIC unity)
# Enable the double-precision asserts (TEST_ASSERT_EQUAL_DOUBLE etc.); harmless
# if unused, and the control-loop math tests will want them.
target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE)

# --- Helper: register one Unity test executable as a ctest case ---------------
# add_host_test(<name> <src> [<more srcs>...])
#   <name>  : test name (also the executable name and the ctest case name)
#   <srcs>  : the test .c plus any production sources it exercises
function(add_host_test test_name)
    add_executable(${test_name} ${ARGN})
    target_link_libraries(${test_name} PRIVATE unity)
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

# --- Test cases ---------------------------------------------------------------
add_host_test(test_dummy test_dummy.c)

# cluster_map: pure lookup table. Links the source under test + data_model hdrs.
add_host_test(test_cluster_map
    test_cluster_map.c
    ../../components/cluster_map/cluster_map.c)
target_include_directories(test_cluster_map PRIVATE
    ../../components/cluster_map/include
    ../../components/data_model/include)

# sensor_state: store + aggregation. Links sensor_state + cluster_map sources,
# and the platform compat shim (host branch) for esp_err_t / mutex / timing.
add_host_test(test_sensor_state
    test_sensor_state.c
    ../../components/sensor_state/sensor_state.c
    ../../components/cluster_map/cluster_map.c)
target_include_directories(test_sensor_state PRIVATE
    ../../components/sensor_state/include
    ../../components/cluster_map/include
    ../../components/data_model/include
    ../../components/platform/include)

# hal_gpio: tested against the host MOCK (hal_gpio_mock.c), never the target
# hal_gpio.c — so this links no ESP-IDF and includes no driver/* headers.
# (Lives in the "bsp" component — see components/bsp/CMakeLists.txt for why.)
add_host_test(test_hal_gpio
    tests/test_hal_gpio.c
    ../../components/bsp/mock/hal_gpio_mock.c)
target_include_directories(test_hal_gpio PRIVATE
    ../../components/bsp/include
    ../../components/bsp/mock
    ../../components/platform/include)

# control_loop: pure relay decisions + source-selection fallback, asserted
# through the GPIO mock. control_loop.c is now store-free (tick moved to
# control_task.c), so this links only the loop + the GPIO mock.
add_host_test(test_control_loop
    tests/test_control_loop.c
    ../../components/control/control_loop.c
    ../../components/bsp/mock/hal_gpio_mock.c)
target_include_directories(test_control_loop PRIVATE
    ../../components/control/include
    ../../components/data_model/include
    ../../components/bsp/include
    ../../components/bsp/mock
    ../../components/platform/include)

# hal_nvs: file-backed mock (/tmp/nvs_sim). No ESP-IDF.
add_host_test(test_hal_nvs
    tests/test_hal_nvs.c
    ../../components/bsp/mock/hal_nvs_mock.c)
target_include_directories(test_hal_nvs PRIVATE
    ../../components/bsp/include
    ../../components/bsp/mock
    ../../components/platform/include)

# hal_timer: deterministic simulated-clock mock (advance/reset helpers).
add_host_test(test_hal_timer
    tests/test_hal_timer.c
    ../../components/bsp/mock/hal_timer_mock.c)
target_include_directories(test_hal_timer PRIVATE
    ../../components/bsp/include
    ../../components/bsp/mock)

# io_scan: pipelined wired-I/O scan against the expander + SHT40 + timer mocks.
# The I/O counts are enabled here (config defaults them to 0) so the expander
# phases run.
add_host_test(test_io_scan
    tests/test_io_scan.c
    ../../components/io_scan/io_scan.c
    ../../components/bsp/mock/hal_i2c_expander_mock.c
    ../../components/bsp/mock/hal_sensor_local_mock.c
    ../../components/bsp/mock/hal_timer_mock.c
    ../../components/sensor_state/sensor_state.c
    ../../components/cluster_map/cluster_map.c)
target_include_directories(test_io_scan PRIVATE
    ../../components/io_scan/include
    ../../components/bsp/include
    ../../components/bsp/mock
    ../../components/sensor_state/include
    ../../components/cluster_map/include
    ../../components/data_model/include
    ../../components/platform/include)
target_compile_definitions(test_io_scan PRIVATE
    IO_MCP23017_COUNT=1 IO_ADS1115_COUNT=1 IO_MCP4728_COUNT=1)

# hal_wdt: no-op call-tracking mock.
add_host_test(test_hal_wdt
    tests/test_hal_wdt.c
    ../../components/bsp/mock/hal_wdt_mock.c)
target_include_directories(test_hal_wdt PRIVATE
    ../../components/bsp/include
    ../../components/bsp/mock
    ../../components/platform/include)

# zigbee_bridge: end-to-end H2-report → frame → decode → cluster map → store →
# aggregation. Real core + framing + sensor_state + cluster_map; the mock injects
# frames in place of the UART RX task; hal_timer mock gives a controllable clock.
add_host_test(test_zigbee_bridge
    tests/test_zigbee_bridge.c
    ../../components/zigbee_bridge/zigbee_bridge.c
    ../../components/zigbee_bridge/mock/zigbee_bridge_mock.c
    ../../components/zigbee_bridge/uart_bridge_framing.c
    ../../components/sensor_state/sensor_state.c
    ../../components/cluster_map/cluster_map.c
    ../../components/bsp/mock/hal_timer_mock.c)
target_include_directories(test_zigbee_bridge PRIVATE
    ../../components/zigbee_bridge/include
    ../../components/zigbee_bridge
    ../../components/zigbee_bridge/mock
    ../../components/sensor_state/include
    ../../components/cluster_map/include
    ../../components/data_model/include
    ../../components/bsp/include
    ../../components/bsp/mock
    ../../components/platform/include)
