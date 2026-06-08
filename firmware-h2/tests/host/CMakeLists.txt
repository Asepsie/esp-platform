# =============================================================================
# firmware-h2 — HOST unit tests
# =============================================================================
# These tests build with the host's native gcc and run as plain executables.
# No ESP-IDF, no hardware. The H2 firmware is thin, so its host-testable logic
# is small and well-bounded:
#
#   * Zigbee cluster handler — maps an incoming ZCL attribute report to a
#     bridge message (pure data transform; no radio needed to test).
#   * UART bridge framing    — [SOF][TYPE][LEN16][PAYLOAD][CRC16] encode/decode
#     and CRC16-CCITT (pure byte math; the highest-value thing to unit test,
#     since both firmwares must agree on the wire format).
#
# The UART bridge protocol header is a byte-identical copy shared with
# firmware-c6; testing the framing on both sides guards against drift.
#
# -----------------------------------------------------------------------------
# How to run
# -----------------------------------------------------------------------------
#   From the repo root (preferred — uses the shared helper):
#       scripts/test-host.sh firmware-h2
#
#   Manually, equivalent steps:
#       cmake -DTARGET_PLATFORM=host -B build/host -G Ninja tests/host
#       cmake --build build/host
#       ctest --test-dir build/host --output-on-failure
#
# -----------------------------------------------------------------------------
# How to add a new test
# -----------------------------------------------------------------------------
#   1. Create tests/host/test_<thing>.c with a main() running Unity
#      (see test_dummy.c for the minimal shape).
#   2. Register it at the bottom of this file, listing the production source(s)
#      it exercises, e.g.:
#          add_host_test(test_uart_bridge_framing
#              test_uart_bridge_framing.c
#              ../../components/uart_bridge/uart_bridge.c)
#          target_include_directories(test_uart_bridge_framing PRIVATE
#              ../../components/uart_bridge/include)
#   ctest discovers it automatically.
# =============================================================================
cmake_minimum_required(VERSION 3.16)

# Host-only tree. Default TARGET_PLATFORM to host and refuse anything else so a
# stray cross/IDF invocation fails loudly rather than building something broken.
if(NOT DEFINED TARGET_PLATFORM)
    set(TARGET_PLATFORM "host")
endif()
if(NOT TARGET_PLATFORM STREQUAL "host")
    message(FATAL_ERROR
        "tests/host builds only with -DTARGET_PLATFORM=host (got '${TARGET_PLATFORM}')")
endif()

project(firmware_h2_host_tests C)

enable_testing()   # turn on ctest support

# --- Unity test framework -----------------------------------------------------
# Vendored under unity/ (see unity/README.md) so host tests have zero ESP-IDF
# dependency. Built once as a static lib and reused by every test executable.
add_library(unity STATIC unity/unity.c)
target_include_directories(unity PUBLIC unity)
target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE)

# --- Helper: register one Unity test executable as a ctest case ---------------
# add_host_test(<name> <src> [<more srcs>...])
function(add_host_test test_name)
    add_executable(${test_name} ${ARGN})
    target_link_libraries(${test_name} PRIVATE unity)
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

# --- Test cases ---------------------------------------------------------------
add_host_test(test_dummy test_dummy.c)

# UART bridge framing/CRC — links the pure framing source under test and adds
# its public header dir. (No IDF needed; uart_bridge_framing.c is plain C.)
add_host_test(test_uart_bridge_framing
    test_uart_bridge_framing.c
    ../../components/uart_bridge/uart_bridge_framing.c)
target_include_directories(test_uart_bridge_framing PRIVATE
    ../../components/uart_bridge/include)

# Planned next (see module checklist in CLAUDE.md):
#   add_host_test(test_cluster_handler ...)
