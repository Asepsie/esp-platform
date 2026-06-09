/**
 * @file hal_wdt.c
 * @brief Task watchdog HAL — ESP32-C6 target implementation.
 *
 * Wraps the ESP-IDF Task Watchdog Timer. The TWDT itself is brought up at boot
 * via CONFIG_ESP_TASK_WDT_INIT; this layer just subscribes/pets tasks.
 */
#include "hal_wdt.h"

#include "esp_task_wdt.h"

esp_err_t hal_wdt_init(void)
{
    // TWDT is initialized at startup (CONFIG_ESP_TASK_WDT_INIT=y). Nothing more
    // to do here; provided as an explicit, idempotent entry point.
    return ESP_OK;
}

esp_err_t hal_wdt_add_task(void)
{
    return esp_task_wdt_add(NULL); // subscribe the calling task
}

esp_err_t hal_wdt_reset(void)
{
    return esp_task_wdt_reset();
}
