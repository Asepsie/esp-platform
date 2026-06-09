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
};

#endif /* HAL_PIN_MAP_H */
