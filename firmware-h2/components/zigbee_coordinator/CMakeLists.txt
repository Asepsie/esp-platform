# ESP-IDF component: zigbee_coordinator — H2 Zigbee 3.0 coordinator.
#
# Split by testability:
#   zigbee_cluster_handler.c — pure C: ZCL attribute -> bridge_sensor_report_t.
#       No esp-zigbee-sdk dependency, so it is also built in host unit tests
#       (tests/host/test_cluster_handler.c).
#   zigbee_coordinator.c     — calls esp-zigbee-sdk (esp_zb_*); target only.
#       Added in a later task; when present it REQUIRES espressif__esp-zigbee-lib.
#
# REQUIRES uart_bridge (bridge_sensor_report_t / protocol header) and platform
# (platform_compat.h -> esp_err_t). No driver/* here — HAL boundary.
idf_component_register(SRCS "zigbee_cluster_handler.c"
                       INCLUDE_DIRS "include"
                       REQUIRES uart_bridge platform)
