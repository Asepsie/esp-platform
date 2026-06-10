/**
 * @file hal_sensor_local.h
 * @brief Onboard SHT40 temperature/humidity sensor HAL (C6).
 *
 * Reads the board-local SHT40 over the shared I2C bus. Provides the control
 * loop's fallback temperature source when Zigbee/H2 data is unavailable.
 *
 * @note A high-precision read takes ~10 ms (measure command + conversion). Call
 *       it on a slow cadence (e.g. every 10 s / every 10th control tick), NOT
 *       every 1 s control cycle.
 */
#ifndef HAL_SENSOR_LOCAL_H
#define HAL_SENSOR_LOCAL_H

#include <stdbool.h>
#include "platform_compat.h"   /* esp_err_t */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the local sensor (brings up I2C, probes the SHT40).
 * @return @c ESP_OK if present, @c ESP_ERR_NOT_FOUND if no sensor responds.
 */
esp_err_t hal_sensor_local_init(void);

/**
 * @brief Read temperature and relative humidity (blocking ~10 ms).
 * @param[out] temp_c  Temperature in °C (non-NULL).
 * @param[out] rh_pct  Relative humidity in % (0–100, non-NULL).
 * @retval ESP_OK              Values written.
 * @retval ESP_ERR_INVALID_ARG NULL pointer.
 * @return Otherwise an @c esp_err_t from the I2C HAL.
 */
esp_err_t hal_sensor_local_read(float *temp_c, float *rh_pct);

/** @brief Whether the local sensor is present/responding. */
bool hal_sensor_local_is_available(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_SENSOR_LOCAL_H */
