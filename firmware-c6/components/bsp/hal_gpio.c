/**
 * @file hal_gpio.c
 * @brief GPIO HAL — ESP32-C6 target implementation.
 *
 * The ONLY file in the project that includes @c driver/gpio.h. Translates the
 * logical @c hal_gpio_id_t API into ESP-IDF GPIO driver calls using the private
 * @c hal_pin_map.h. A shadow array holds the last commanded level so
 * @c hal_gpio_get() reads back reliably for output lines.
 */
#include "hal_gpio.h"
#include "hal_pin_map.h"

#include "driver/gpio.h"

/** @brief Last commanded level per line (outputs are not read from silicon). */
static bool s_level[HAL_GPIO_COUNT];

/** @brief True between a successful hal_gpio_init() and any de-init. */
static bool s_initialized;

esp_err_t hal_gpio_init(void)
{
    for (int id = 0; id < HAL_GPIO_COUNT; id++) {
        const gpio_num_t pin = (gpio_num_t)HAL_PIN_MAP[id].gpio_num;
        const gpio_config_t cfg = {
            .pin_bit_mask = (1ULL << pin),
            .mode         = GPIO_MODE_OUTPUT,
            .pull_up_en   = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE,
        };
        esp_err_t err = gpio_config(&cfg);
        if (err != ESP_OK) {
            return err;
        }
        /* Safe default: all lines LOW (relays off, LED off, H2 held in reset). */
        err = gpio_set_level(pin, 0);
        if (err != ESP_OK) {
            return err;
        }
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
    esp_err_t err = gpio_set_level((gpio_num_t)HAL_PIN_MAP[id].gpio_num, level ? 1 : 0);
    if (err == ESP_OK) {
        s_level[id] = level;
    }
    return err;
}

esp_err_t hal_gpio_get(hal_gpio_id_t id, bool *level)
{
    if (id < 0 || id >= HAL_GPIO_COUNT || level == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *level = s_level[id];
    return ESP_OK;
}
