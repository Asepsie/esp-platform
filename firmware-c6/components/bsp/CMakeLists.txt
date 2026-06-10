# ESP-IDF component: bsp — board support / hardware abstraction layer (C6).
#
# NOTE: named "bsp", NOT "hal". ESP-IDF ships a reserved core component called
# "hal"; a same-named project component overrides it and breaks the build
# (mbedtls/drivers need its headers). The HAL *API* keeps the hal_* prefix
# (hal_gpio.h, hal_gpio_*()) — only the IDF component name differs.
#
# Only the target implementation (hal_gpio.c) is compiled into the firmware;
# it is the sole includer of driver/gpio.h. The mock under mock/ is for host
# unit tests only and is pulled in by tests/host, never by this component.
# Pin numbers live in private/ (PRIV_INCLUDE_DIRS) so they stay HAL-internal.
idf_component_register(SRCS "hal_gpio.c" "hal_nvs.c" "hal_timer.c" "hal_wdt.c"
                            "hal_uart.c" "hal_uart_mstp.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "private"
                       REQUIRES platform
                       PRIV_REQUIRES esp_driver_gpio nvs_flash esp_timer esp_system esp_driver_uart)
