/**
 * @file hal_gpio_mock.c
 * @brief Host mock of the H2 GPIO HAL — array-backed, no hardware.
 */
#include "hal_gpio_mock.h"

static bool s_level[HAL_GPIO_COUNT];
static bool s_initialized;

esp_err_t hal_gpio_init(void)
{
    for (int id = 0; id < HAL_GPIO_COUNT; id++) {
        s_level[id] = false;
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
