/**
 * @file hal_pin_map.h
 * @brief Physical pin assignments — PRIVATE to the H2 bsp component.
 *
 * Plain integer macros (no `gpio_num_t`) so this header pulls in no driver
 * dependency; only the target `.c` files include the drivers. Included solely by
 * `hal_uart.c` and `hal_gpio.c`.
 *
 * ⚠ Pin numbers are firmware DEFAULTS pending the schematic
 * (docs/hardware/hardware-spec-v2.md §6, H2 map). UART1 is used because UART0 is
 * the log console. CONFIRM AGAINST THE BOARD before bring-up.
 */
#ifndef HAL_PIN_MAP_H
#define HAL_PIN_MAP_H

// Bridge UART (UART1): H2 TX -> C6 GPIO17 (RX); H2 RX <- C6 GPIO16 (TX).
#define HAL_UART_TX_GPIO     5
#define HAL_UART_RX_GPIO     4

// Status LED (Zigbee network status).
#define HAL_STATUS_LED_GPIO  8

#endif /* HAL_PIN_MAP_H */
