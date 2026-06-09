/**
 * @file hal_gpio_mock.h
 * @brief Host mock for the H2 GPIO HAL — state inspection helpers.
 */
#ifndef HAL_GPIO_MOCK_H
#define HAL_GPIO_MOCK_H

#include "hal_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Captured level of a line (false if @p id out of range). */
bool hal_gpio_mock_get_state(hal_gpio_id_t id);

/** @brief Reset all captured state to LOW and clear init (call from setUp()). */
void hal_gpio_mock_reset_all(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_GPIO_MOCK_H */
