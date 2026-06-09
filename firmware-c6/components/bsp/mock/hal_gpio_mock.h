/**
 * @file hal_gpio_mock.h
 * @brief Host test mock for the GPIO HAL — extra inspection helpers.
 *
 * The mock (@c hal_gpio_mock.c) provides the full @c hal_gpio.h API backed by
 * an in-memory array instead of hardware, so logic that drives GPIO can be unit
 * tested with plain gcc (no ESP-IDF). This header adds test-only helpers to
 * inspect and reset captured state. It is NOT part of the production HAL API.
 */
#ifndef HAL_GPIO_MOCK_H
#define HAL_GPIO_MOCK_H

#include <stdbool.h>
#include "hal_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read the captured level of a line, bypassing the HAL API.
 *
 * @param id Logical line identifier. Out-of-range ids return @c false.
 * @return The captured level (@c true = high).
 */
bool hal_gpio_mock_get_state(hal_gpio_id_t id);

/**
 * @brief Reset all captured line state to LOW and clear the init flag.
 *
 * Call from a test's @c setUp() so each test starts from a known state.
 */
void hal_gpio_mock_reset_all(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_GPIO_MOCK_H */
