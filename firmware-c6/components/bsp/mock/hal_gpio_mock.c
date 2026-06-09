/**
 * @file hal_gpio_mock.c
 * @brief Host test mock implementation of the GPIO HAL.
 *
 * Implements the full @c hal_gpio.h API by capturing line state in a static
 * array — no hardware, no ESP-IDF (compiles with plain gcc). Mirrors the target
 * defaults (all lines LOW after init) so behavioural tests match the device.
 * Link this instead of @c hal_gpio.c in host unit tests.
 */
#include "hal_gpio_mock.h"

/** @brief Captured level per logical line. */
static bool s_level[HAL_GPIO_COUNT];

/** @brief Set once hal_gpio_init() has run (mirrors target state guard). */
static bool s_initialized;

/* --- production HAL API (mock-backed) ------------------------------------- */

esp_err_t hal_gpio_init(void)
{
    for (int id = 0; id < HAL_GPIO_COUNT; id++) {
        s_level[id] = false; /* safe default: LOW, same as target */
    }
    s_initialized = true;
    return ESP_OK;
}

esp_err_t hal_gpio_set(hal_gpio_id_t id, bool level)
{
    if (id < 0 || id >= HAL_GPIO_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    s_level[id] = level;
    return ESP_OK;
}

esp_err_t hal_gpio_get(hal_gpio_id_t id, bool *level)
{
    if (id < 0 || id >= HAL_GPIO_COUNT || level == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *level = s_level[id];
    return ESP_OK;
}

/* --- test-only helpers ---------------------------------------------------- */

bool hal_gpio_mock_get_state(hal_gpio_id_t id)
{
    if (id < 0 || id >= HAL_GPIO_COUNT) {
        return false;
    }
    return s_level[id];
}

void hal_gpio_mock_reset_all(void)
{
    for (int id = 0; id < HAL_GPIO_COUNT; id++) {
        s_level[id] = false;
    }
    s_initialized = false;
}
