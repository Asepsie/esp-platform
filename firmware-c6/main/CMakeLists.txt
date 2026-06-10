idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES sensor_state bsp control zigbee_bridge)
