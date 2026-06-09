/**
 * @file hal_wdt.h
 * @brief Task watchdog HAL (C6) — public interface.
 *
 * Logical wrapper over the ESP-IDF Task Watchdog Timer that completes RT-07:
 * tasks register once and "pet" the dog within their deadline through this API
 * instead of calling @c esp_task_wdt_* directly. The host mock counts calls so
 * tests can assert registration/reset behaviour without a real watchdog.
 *
 * @see docs/architecture/rt-rules-v2.md — RT-07 (watchdog registration).
 */
#ifndef HAL_WDT_H
#define HAL_WDT_H

#include "platform_compat.h"   /* esp_err_t (target: esp_err.h; host: shim) */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Ensure the task watchdog subsystem is ready.
 *
 * The TWDT is initialized at boot (CONFIG_ESP_TASK_WDT_INIT); this is a safe,
 * idempotent entry point for code that wants to be explicit.
 *
 * @return @c ESP_OK on success.
 */
esp_err_t hal_wdt_init(void);

/**
 * @brief Subscribe the calling task to the watchdog.
 *
 * After this, the task must call @c hal_wdt_reset() within the watchdog timeout
 * or the system resets.
 *
 * @return @c ESP_OK on success, or an @c esp_err_t from the backend.
 */
esp_err_t hal_wdt_add_task(void);

/**
 * @brief Reset (pet) the watchdog for the calling task.
 * @return @c ESP_OK on success, or an @c esp_err_t from the backend.
 */
esp_err_t hal_wdt_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WDT_H */
