/**
 * @file hal_wdt_mock.h
 * @brief Host mock for the watchdog HAL — call-tracking helpers.
 *
 * The mock (@c hal_wdt_mock.c) is a no-op implementation of @c hal_wdt.h that
 * records the configured timeout, task registration, and reset count, so tests
 * can assert watchdog usage without a real TWDT. Not part of the production API.
 */
#ifndef HAL_WDT_MOCK_H
#define HAL_WDT_MOCK_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Number of @c hal_wdt_reset() calls since reset. */
uint32_t hal_wdt_mock_get_reset_count(void);

/** @brief True if @c hal_wdt_add_current_task() has been called since reset. */
bool hal_wdt_mock_is_task_registered(void);

/** @brief The timeout (seconds) last passed to @c hal_wdt_init(). */
uint32_t hal_wdt_mock_get_timeout_s(void);

/** @brief Clear all tracked state (call from a test's setUp()). */
void hal_wdt_mock_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WDT_MOCK_H */
