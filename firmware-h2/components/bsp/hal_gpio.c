/**
 * @file hal_gpio.c
 * @brief GPIO HAL — ESP32-H2 target implementation (status LED).
 *
 * Sole includer of `driver/gpio.h` on the H2. Maps logical lines to physical
 * pins via the private pin map and shadows the last commanded level.
 */
#include "hal_gpio.h"
#include "hal_pin_map.h"

#include "driver/gpio.h"

static bool s_level[HAL_GPIO_COUNT];
static bool s_initialized;

// Physical pin for a logical line, or -1 if unknown.
static int pin_for(hal_gpio_id_t id)
{
    switch (id) {
    case HAL_GPIO_STATUS_LED: return HAL_STATUS_LED_GPIO;
    default:                  return -1;
    }
}

esp_err_t hal_gpio_init(void)
{
    for (int id = 0; id < HAL_GPIO_COUNT; id++) {
        const gpio_num_t pin = (gpio_num_t)pin_for((hal_gpio_id_t)id);
        const gpio_config_t cfg = {
            .pin_bit_mask = (1ULL << pin),
            .mode         = GPIO_MODE_OUTPUT,
            .pull_up_en   = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE,
        };
        esp_err_t err = gpio_config(&cfg);
        if (err != ESP_OK) return err;
        err = gpio_set_level(pin, 0); // default LOW (LED off)
        if (err != ESP_OK) return err;
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
    esp_err_t err = gpio_set_level((gpio_num_t)pin_for(id), level ? 1 : 0);
    if (err == ESP_OK) {
        s_level[id] = level;
    }
    return err;
}
