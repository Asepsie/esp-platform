/**
 * @file hal_timer_mock.c
 * @brief Host mock of the timing HAL — real monotonic clock + sleep.
 *
 * Uses CLOCK_MONOTONIC and usleep() so host tests observe genuine elapsed time
 * (the timer tests assert monotonicity and delay duration). Plain POSIX, no
 * ESP-IDF. No extra helpers, so there is no hal_timer_mock.h.
 */
#include "hal_timer.h"

#include <time.h>
#include <unistd.h>
#include <stdint.h>

uint32_t hal_timer_get_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u);
}

void hal_timer_delay_ms(uint32_t ms)
{
    usleep((useconds_t)ms * 1000u);
}
