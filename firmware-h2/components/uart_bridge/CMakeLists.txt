# ESP-IDF component: uart_bridge (H2 side of the C6 <-> H2 link).
#
#   uart_bridge_framing.c — pure encode/decode/CRC (also built in host tests)
#   uart_bridge.c         — RX task + TX/heartbeat; UART I/O via hal_uart (bsp)
#
# REQUIRES bsp for hal_uart.h (no direct driver/uart.h here — HAL boundary).
# freertos/esp_common (mutex, tasks, logging) are default requirements.
idf_component_register(SRCS "uart_bridge_framing.c" "uart_bridge.c"
                       INCLUDE_DIRS "include"
                       REQUIRES bsp)
