/**
 * @file hal_wdt.h
 * @brief Task watchdog HAL (C6) — public interface.
 *
 * Logical wrapper over the ESP-IDF Task Watchdog Timer that completes RT-07:
 * code configures the timeout, registers tasks, and "pets" the dog through this
 * API instead of calling @c esp_task_wdt_* directly. The host mock tracks calls
 * so tests can assert registration/reset behaviour without a real watchdog.
 *
 * @see docs/architecture/rt-rules-v2.md — RT-07 (watchdog registration).
 */
#ifndef HAL_WDT_H
#define HAL_WDT_H

#include <stdint.h>
#include "platform_compat.h"   /* esp_err_t (target: esp_err.h; host: shim) */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configure the task watchdog timeout.
 *
 * The TWDT is brought up at boot (CONFIG_ESP_TASK_WDT_INIT); this reconfigures
 * it to @p timeout_s and keeps watching the idle task. A subscribed task that
 * fails to reset within the timeout triggers a panic + reset.
 *
 * @param timeout_s Watchdog timeout in seconds.
 * @return @c ESP_OK on success, or an @c esp_err_t from the backend.
 */
esp_err_t hal_wdt_init(uint32_t timeout_s);

/**
 * @brief Subscribe the calling task to the watchdog.
 *
 * After this the task must call @c hal_wdt_reset() within the configured
 * timeout.
 *
 * @return @c ESP_OK on success, or an @c esp_err_t from the backend.
 */
esp_err_t hal_wdt_add_current_task(void);

/**
 * @brief Reset (pet) the watchdog for the calling task.
 * @return @c ESP_OK on success, or an @c esp_err_t from the backend.
 */
esp_err_t hal_wdt_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WDT_H */
