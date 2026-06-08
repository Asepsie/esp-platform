# ESP-IDF component: uart_bridge (H2 side of the C6 <-> H2 link).
#
#   uart_bridge_framing.c — pure encode/decode/CRC (also built in host tests)
#   uart_bridge.c         — UART1 driver + RX task + TX/heartbeat helpers
#
# REQUIRES esp_driver_uart for the UART driver API; freertos/esp_common (mutex,
# tasks, esp_check, logging) are part of the default component requirements.
idf_component_register(SRCS "uart_bridge_framing.c" "uart_bridge.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_driver_uart)
