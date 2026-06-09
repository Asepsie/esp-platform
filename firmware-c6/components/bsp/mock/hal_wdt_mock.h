/**
 * @file hal_wdt_mock.h
 * @brief Host mock for the watchdog HAL — call-tracking helpers.
 *
 * The mock (@c hal_wdt_mock.c) is a no-op implementation of @c hal_wdt.h that
 * counts calls, so tests can assert that tasks register and pet the watchdog
 * without a real TWDT. Not part of the production API.
 */
#ifndef HAL_WDT_MOCK_H
#define HAL_WDT_MOCK_H

#include "hal_wdt.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Reset all call counters and clear state (call from setUp()). */
void hal_wdt_mock_reset(void);

/** @brief Number of @c hal_wdt_init() calls since reset. */
int hal_wdt_mock_init_count(void);

/** @brief Number of @c hal_wdt_add_task() calls since reset. */
int hal_wdt_mock_add_count(void);

/** @brief Number of @c hal_wdt_reset() calls since reset. */
int hal_wdt_mock_reset_count(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WDT_MOCK_H */
