/**
 * @file hal_timer.h
 * @brief Monotonic timing HAL (C6) — public interface.
 *
 * The platform timing abstraction that completes RT-08: logic reads time and
 * delays through these calls instead of touching @c esp_timer / FreeRTOS
 * directly, so it runs unchanged on target and host. Target maps to
 * @c esp_timer_get_time() + @c vTaskDelay(); the host mock maps to
 * @c CLOCK_MONOTONIC + @c usleep().
 *
 * @see docs/architecture/rt-rules-v2.md — RT-08 (abstract timing).
 */
#ifndef HAL_TIMER_H
#define HAL_TIMER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Milliseconds since boot from a monotonic clock.
 *
 * Never goes backwards. Wraps after ~49.7 days (uint32 ms); callers needing
 * longer spans must handle wrap with unsigned subtraction.
 *
 * @return Monotonic milliseconds.
 */
uint32_t hal_timer_get_ms(void);

/**
 * @brief Microseconds since boot from the monotonic clock.
 *
 * For short-interval measurements (e.g. I/O scan duration). Wraps after
 * ~71 minutes (uint32 µs); use unsigned subtraction for elapsed time.
 *
 * @return Monotonic microseconds.
 */
uint32_t hal_timer_get_us(void);

/**
 * @brief Block the caller for at least @p ms milliseconds.
 *
 * On target this yields to the scheduler (`vTaskDelay`); on host it sleeps the
 * process. Not for sub-tick precision — it is a cooperative delay.
 *
 * @param ms Milliseconds to delay (0 returns promptly).
 */
void hal_timer_delay_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* HAL_TIMER_H */
