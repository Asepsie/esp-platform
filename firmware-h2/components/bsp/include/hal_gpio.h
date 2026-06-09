/**
 * @file hal_gpio.h
 * @brief GPIO HAL (H2) — public interface.
 *
 * Minimal for now: the H2 status LED (Zigbee network status). Same logical-id
 * pattern as the C6 HAL so it can grow without API churn. No driver headers here.
 */
#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdbool.h>
#include "platform_compat.h"   /* esp_err_t (target: esp_err.h; host: shim) */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Logical GPIO lines on the H2. @c HAL_GPIO_COUNT is a sentinel. */
typedef enum {
    HAL_GPIO_STATUS_LED = 0, /**< Zigbee network status LED (active high). */
    HAL_GPIO_COUNT,          /**< Number of logical lines (sentinel).      */
} hal_gpio_id_t;

/**
 * @brief Configure all H2 GPIO lines as outputs, driven LOW (off).
 * @return @c ESP_OK on success, or an @c esp_err_t from the driver.
 */
esp_err_t hal_gpio_init(void);

/**
 * @brief Set a logical line level.
 * @param id    Logical line (< @c HAL_GPIO_COUNT).
 * @param level @c true = high, @c false = low.
 * @retval ESP_OK / ESP_ERR_INVALID_ARG / ESP_ERR_INVALID_STATE
 */
esp_err_t hal_gpio_set(hal_gpio_id_t id, bool level);

#ifdef __cplusplus
}
#endif

#endif /* HAL_GPIO_H */
