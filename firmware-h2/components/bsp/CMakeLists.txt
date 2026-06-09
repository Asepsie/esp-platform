# ESP-IDF component: bsp — H2 hardware abstraction layer.
#
# NOTE: named "bsp", NOT "hal" — ESP-IDF reserves the component name "hal"
# (a same-named project component breaks the build). The HAL *API* keeps the
# hal_* prefix (hal_uart.h, hal_gpio.h). See docs/architecture/hal-design.md §3.
#
# hal_uart.c is the sole includer of driver/uart.h on the H2 (the boundary fix
# that removed driver/uart.h from uart_bridge.c); hal_gpio.c is the sole
# includer of driver/gpio.h. Mocks under mock/ are host-test only.
idf_component_register(SRCS "hal_uart.c" "hal_gpio.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "private"
                       REQUIRES platform
                       PRIV_REQUIRES esp_driver_uart esp_driver_gpio)
