/**
 * @file hal_wdt_mock.c
 * @brief Host mock of the watchdog HAL — no-op with call tracking.
 *
 * Implements @c hal_wdt.h without a real watchdog and records how many times
 * each entry point was called, for test assertions. Plain C, no ESP-IDF.
 */
#include "hal_wdt_mock.h"

static int s_init_count;
static int s_add_count;
static int s_reset_count;

esp_err_t hal_wdt_init(void)
{
    s_init_count++;
    return ESP_OK;
}

esp_err_t hal_wdt_add_task(void)
{
    s_add_count++;
    return ESP_OK;
}

esp_err_t hal_wdt_reset(void)
{
    s_reset_count++;
    return ESP_OK;
}

void hal_wdt_mock_reset(void)
{
    s_init_count = 0;
    s_add_count = 0;
    s_reset_count = 0;
}

int hal_wdt_mock_init_count(void)  { return s_init_count; }
int hal_wdt_mock_add_count(void)   { return s_add_count; }
int hal_wdt_mock_reset_count(void) { return s_reset_count; }
