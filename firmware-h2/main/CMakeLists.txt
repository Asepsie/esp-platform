idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES uart_bridge zigbee_coordinator nvs_flash)
