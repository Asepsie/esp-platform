/**
 * @file hal_i2c.h
 * @brief Minimal I2C master HAL (C6) — shared expansion bus.
 *
 * Address-oriented wrapper over the ESP-IDF I2C-master driver for the shared
 * expansion bus (GPIO8/9 @ 400 kHz). Device handles are created lazily per
 * 7-bit address. Used by hal_sensor_local (SHT40) and the I/O expanders.
 * Callers never include driver/i2c_master.h.
 */
#ifndef HAL_I2C_H
#define HAL_I2C_H

#include <stddef.h>
#include <stdint.h>
#include "platform_compat.h"   /* esp_err_t */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the shared I2C master bus (idempotent).
 * @return @c ESP_OK on success, or an @c esp_err_t from the driver.
 */
esp_err_t hal_i2c_init(void);

/**
 * @brief Write @p len bytes to the 7-bit device at @p addr.
 * @return @c ESP_OK on success, else an @c esp_err_t (ESP_ERR_INVALID_STATE if
 *         the bus is not initialized).
 */
esp_err_t hal_i2c_write(uint8_t addr, const uint8_t *data, size_t len);

/**
 * @brief Read @p len bytes from the 7-bit device at @p addr.
 * @return @c ESP_OK on success, else an @c esp_err_t.
 */
esp_err_t hal_i2c_read(uint8_t addr, uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* HAL_I2C_H */
