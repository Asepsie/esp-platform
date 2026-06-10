# ESP-IDF component: zigbee_bridge — C6 side of the C6<->H2 UART bridge.
#
#   zigbee_bridge.c        — core decode/dispatch + H2 liveness (host + target)
#   zigbee_bridge_io.c     — target RX task + command TX (uses bsp hal_uart)
#   uart_bridge_framing.c  — shared pure framing (byte-identical to firmware-h2)
#
# The shared wire contract include/uart_bridge_protocol.h is a byte-identical
# copy of the firmware-h2 one (the bridge protocol is duplicated per project).
# Host tests replace zigbee_bridge_io.c with mock/zigbee_bridge_mock.c.
idf_component_register(SRCS "zigbee_bridge.c" "zigbee_bridge_io.c" "uart_bridge_framing.c"
                       INCLUDE_DIRS "include"
                       REQUIRES bsp sensor_state cluster_map data_model platform)
