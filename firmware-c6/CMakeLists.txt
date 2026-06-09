# firmware-c6 — ESP32-C6 primary MCU
# Top-level project CMake for the C6 firmware (BACnet, control, UI, OTA).
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Make config/thermostat_config.h reachable from every component as a global
# include (the single source of compile-time constants). Must precede project().
idf_build_set_property(COMPILE_OPTIONS "-I${CMAKE_CURRENT_LIST_DIR}/config" APPEND)

project(firmware-c6)
