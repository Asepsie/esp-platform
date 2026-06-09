/**
 * @file hal_wdt.c
 * @brief Task watchdog HAL — ESP32-C6 target implementation.
 *
 * Wraps the ESP-IDF Task Watchdog Timer. The TWDT is initialized at boot via
 * CONFIG_ESP_TASK_WDT_INIT; hal_wdt_init() reconfigures its timeout, and the
 * subscribe/reset calls map straight to the driver.
 */
#include "hal_wdt.h"

#include "esp_task_wdt.h"

esp_err_t hal_wdt_init(uint32_t timeout_s)
{
    // Reconfigure the already-initialized TWDT to the requested timeout,
    // keeping the core-0 idle task subscribed and panic-on-timeout behaviour.
    const esp_task_wdt_config_t cfg = {
        .timeout_ms     = timeout_s * 1000u,
        .idle_core_mask = (1 << 0),  // C6 is single-core
        .trigger_panic  = true,
    };
    return esp_task_wdt_reconfigure(&cfg);
}

esp_err_t hal_wdt_add_current_task(void)
{
    return esp_task_wdt_add(NULL); // subscribe the calling task
}

esp_err_t hal_wdt_reset(void)
{
    return esp_task_wdt_reset();
}
