# ESP-IDF component: io_scan — pipelined wired-I/O scan (runs in control tick).
# Drives the I2C expanders + SHT40 via the bsp HALs; pushes the local sensor
# reading into sensor_state. Chip presence is config-gated (IO_*_COUNT).
idf_component_register(SRCS "io_scan.c"
                       INCLUDE_DIRS "include"
                       REQUIRES bsp sensor_state data_model platform)
