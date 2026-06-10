/**
 * @file hal_pin_map.h
 * @brief Physical GPIO pin assignments — PRIVATE to the HAL component.
 *
 * This header maps each logical @c hal_gpio_id_t to a physical ESP32-C6 GPIO
 * number. It is intentionally kept under @c components/bsp/private/ and is
 * included ONLY by the target implementation (@c hal_gpio.c). Application code
 * and other components must never see physical pin numbers (CLAUDE.md — HAL
 * boundary). Pin numbers per docs/hardware/hardware-spec-v2.md §6 (C6 map).
 *
 * Pin numbers are plain integers (not @c gpio_num_t) so this header carries no
 * ESP-IDF driver dependency — only @c hal_gpio.c includes the GPIO driver.
 */
#ifndef HAL_PIN_MAP_H
#define HAL_PIN_MAP_H

#include <stdint.h>
#include "hal_gpio.h"
#include "thermostat_config.h"   /* H2_EN_GPIO (board pin constants) */

/* BACnet MS/TP over RS-485 (used by hal_uart_mstp.c). The DE line is also
 * exposed as the logical HAL_GPIO_RS485_DE so direction control goes through
 * hal_gpio. PINMAP_MSTP_UART is a plain int (UART_NUM_0) to keep this header
 * free of any ESP-IDF driver dependency (hal-design.md §5.2). */
#define PINMAP_MSTP_TX      3
#define PINMAP_MSTP_RX      4
#define PINMAP_RS485_DE     5
#define PINMAP_MSTP_UART    0       /* UART_NUM_0 */
#define PINMAP_MSTP_BAUD    38400   /* configurable 9600–76800 */

/* Onboard SHT40 temp/RH sensor. Shares the I2C expansion bus (GPIO8/9) with the
 * CST816 touch (TH-DISPLAY) and the MCP23017/ADS1115/MCP4728 expanders. */
#define PINMAP_SHT40_ADDR   0x44

/** @brief Physical definition of one logical GPIO line. */
typedef struct {
    uint8_t gpio_num; /**< ESP32-C6 physical GPIO number. */
} hal_pin_def_t;

/**
 * @brief Logical line → physical pin table, indexed by @c hal_gpio_id_t.
 *
 * GPIO15 (status LED) and GPIO0–2 (relays) are strapping-sensitive on the C6;
 * the board uses external pulls per the hardware spec.
 */
static const hal_pin_def_t HAL_PIN_MAP[HAL_GPIO_COUNT] = {
    [HAL_GPIO_RELAY_HEAT] = { .gpio_num = 0  }, /* RELAY_HEAT */
    [HAL_GPIO_RELAY_COOL] = { .gpio_num = 1  }, /* RELAY_COOL */
    [HAL_GPIO_RELAY_FAN]  = { .gpio_num = 2  }, /* RELAY_FAN  */
    [HAL_GPIO_STATUS_LED] = { .gpio_num = 15 }, /* STATUS_LED (strapping, ext pull-down) */
    [HAL_GPIO_H2_EN]      = { .gpio_num = H2_EN_GPIO }, /* H2_EN (hard reset via 10kΩ) */
    [HAL_GPIO_RS485_DE]   = { .gpio_num = PINMAP_RS485_DE }, /* RS-485 DE+RE (TX when high) */
};

#endif /* HAL_PIN_MAP_H */
