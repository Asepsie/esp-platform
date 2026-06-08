# ESP-IDF component: platform — target/host portability shim (header only).
# REQUIRES propagate publicly so consumers get esp_timer/freertos transitively.
idf_component_register(INCLUDE_DIRS "include"
                       REQUIRES esp_timer)
