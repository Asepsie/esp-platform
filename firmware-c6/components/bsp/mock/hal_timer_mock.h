/**
 * @file hal_timer_mock.h
 * @brief Host mock for the timing HAL — deterministic simulated clock helpers.
 *
 * The mock (@c hal_timer_mock.c) backs @c hal_timer.h with a controllable
 * counter instead of a real clock, so time-dependent logic is tested
 * deterministically (no wall-clock sleeps, no flakiness). @c hal_timer_delay_ms()
 * advances the simulated clock by the requested amount; tests can also advance
 * it explicitly.
 */
#ifndef HAL_TIMER_MOCK_H
#define HAL_TIMER_MOCK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Advance the simulated clock by @p ms milliseconds.
 *
 * Subsequent @c hal_timer_get_ms() calls reflect the advance. Use to drive
 * time-based logic forward in tests without sleeping.
 *
 * @param ms Milliseconds to advance.
 */
void hal_timer_mock_advance_ms(uint32_t ms);

/** @brief Reset the simulated clock to 0 (call from a test's setUp()). */
void hal_timer_mock_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_TIMER_MOCK_H */
