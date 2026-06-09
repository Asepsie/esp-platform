/**
 * @file hal_wdt_mock.c
 * @brief Host mock of the watchdog HAL — no-op with call tracking.
 *
 * Implements @c hal_wdt.h without a real watchdog and records the configured
 * timeout, task registration, and reset count for test assertions. Plain C.
 */
#include "hal_wdt.h"
#include "hal_wdt_mock.h"

static uint32_t s_timeout_s;
static bool     s_task_registered;
static uint32_t s_reset_count;

esp_err_t hal_wdt_init(uint32_t timeout_s)
{
    s_timeout_s = timeout_s;
    return ESP_OK;
}

esp_err_t hal_wdt_add_current_task(void)
{
    s_task_registered = true;
    return ESP_OK;
}

esp_err_t hal_wdt_reset(void)
{
    s_reset_count++;
    return ESP_OK;
}

uint32_t hal_wdt_mock_get_reset_count(void)  { return s_reset_count; }
bool     hal_wdt_mock_is_task_registered(void) { return s_task_registered; }
uint32_t hal_wdt_mock_get_timeout_s(void)    { return s_timeout_s; }

void hal_wdt_mock_reset(void)
{
    s_timeout_s = 0;
    s_task_registered = false;
    s_reset_count = 0;
}
