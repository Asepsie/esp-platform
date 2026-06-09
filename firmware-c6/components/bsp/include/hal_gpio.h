/**
 * @file hal_gpio.h
 * @brief GPIO hardware abstraction layer — public interface (C6).
 *
 * Application code drives relays, the status LED, and the H2 enable line
 * through this logical API only. Physical GPIO numbers live in the private
 * @c hal_pin_map.h and are never exported. No ESP-IDF driver header is
 * included here, so this interface compiles unchanged against the target
 * driver implementation (@c hal_gpio.c) or the host mock (@c hal_gpio_mock.c).
 *
 * @see CLAUDE.md — "HAL boundary" non-negotiable rules.
 */
#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdbool.h>
#include "platform_compat.h"   /* esp_err_t (target: esp_err.h; host: shim) */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Logical GPIO line identifiers.
 *
 * Order is significant: values index the private pin map. @c HAL_GPIO_COUNT
 * is a sentinel (number of lines) and is not itself a valid line.
 */
typedef enum {
    HAL_GPIO_RELAY_HEAT = 0, /**< Heat-call relay output (active high, off=low). */
    HAL_GPIO_RELAY_COOL,     /**< Cool-call relay output (active high, off=low). */
    HAL_GPIO_RELAY_FAN,      /**< Fan relay output (active high, off=low).       */
    HAL_GPIO_STATUS_LED,     /**< Status LED output (active high, off=low).      */
    HAL_GPIO_H2_EN,          /**< ESP32-H2 enable/reset line (active high=run).  */
    HAL_GPIO_COUNT,          /**< Number of logical lines (sentinel).            */
} hal_gpio_id_t;

/**
 * @brief Configure all HAL GPIO lines and drive them to their safe default.
 *
 * Every line is configured as an output and driven LOW (relays off, LED off,
 * H2 held in reset). The startup sequence must call
 * @c hal_gpio_set(HAL_GPIO_H2_EN, true) to release the H2 from reset.
 *
 * @return @c ESP_OK on success, or an @c esp_err_t from the underlying driver.
 */
esp_err_t hal_gpio_init(void);

/**
 * @brief Set a logical GPIO line to the given level.
 *
 * @param id    Logical line identifier (< @c HAL_GPIO_COUNT).
 * @param level @c true drives the pin high, @c false drives it low.
 * @retval ESP_OK              Level applied.
 * @retval ESP_ERR_INVALID_ARG @p id is out of range.
 */
esp_err_t hal_gpio_set(hal_gpio_id_t id, bool level);

/**
 * @brief Read back the last commanded level of a logical GPIO line.
 *
 * All current lines are outputs, so this returns the level last written via
 * @c hal_gpio_set() (or the init default).
 *
 * @param[in]  id    Logical line identifier (< @c HAL_GPIO_COUNT).
 * @param[out] level Receives the current level (@c true = high).
 * @retval ESP_OK              @p level written.
 * @retval ESP_ERR_INVALID_ARG @p id is out of range or @p level is NULL.
 */
esp_err_t hal_gpio_get(hal_gpio_id_t id, bool *level);

#ifdef __cplusplus
}
#endif

#endif /* HAL_GPIO_H */
