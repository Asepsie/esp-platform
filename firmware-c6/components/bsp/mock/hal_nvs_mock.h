/**
 * @file hal_nvs_mock.h
 * @brief Host mock for the NVS HAL — extra test helpers.
 *
 * The mock (@c hal_nvs_mock.c) implements the full @c hal_nvs.h API backed by
 * files under /tmp/nvs_sim/, so persistence can be unit tested with plain gcc.
 * This header adds a reset helper for test isolation. Not part of the API.
 */
#ifndef HAL_NVS_MOCK_H
#define HAL_NVS_MOCK_H

#include "hal_nvs.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Wipe all simulated NVS state (deletes the backing directory contents).
 *
 * Call from a test's @c setUp() so each test starts from an empty store. Also
 * clears the initialized flag, so @c hal_nvs_init() must be called afterwards.
 */
void hal_nvs_mock_reset(void);

/**
 * @brief Advance the mock's simulated clock by @p ms milliseconds.
 *
 * Drives write coalescing in host tests: if there are pending writes and the
 * elapsed time since the last write reaches the 2 s window, a commit occurs.
 */
void hal_nvs_mock_advance_ms(uint32_t ms);

/**
 * @brief Arm a simulated corruption so the next @c hal_nvs_init() recovers.
 *
 * Makes the next init behave as if the partition had no free pages: it wipes
 * the store (factory defaults), resets the write counter, and reports
 * @c recovered == true.
 *
 * @param corrupt @c true to arm recovery on the next init.
 */
void hal_nvs_mock_set_corrupt(bool corrupt);

#ifdef __cplusplus
}
#endif

#endif /* HAL_NVS_MOCK_H */
