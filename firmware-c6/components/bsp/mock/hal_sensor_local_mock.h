/**
 * @file hal_sensor_local_mock.h
 * @brief Host mock for the local sensor HAL — set values + availability.
 *
 * The mock (@c hal_sensor_local_mock.c) returns configurable temperature/RH and
 * a configurable availability so the io_scan and control-loop fallback paths can
 * be tested deterministically on host.
 */
#ifndef HAL_SENSOR_LOCAL_MOCK_H
#define HAL_SENSOR_LOCAL_MOCK_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Set the values returned by hal_sensor_local_read() (marks available). */
void hal_sensor_local_mock_set(float temp, float rh);

/** @brief Set whether the sensor reports as available/present. */
void hal_sensor_local_mock_set_available(bool available);

#ifdef __cplusplus
}
#endif

#endif /* HAL_SENSOR_LOCAL_MOCK_H */
