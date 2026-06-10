/**
 * @file hal_timer_mock.c
 * @brief Host mock of the timing HAL — deterministic simulated clock.
 *
 * Time is a plain counter, not the wall clock: get_ms() reads it, delay_ms()
 * advances it (no real sleep), and hal_timer_mock_advance_ms() advances it
 * explicitly. This makes time-dependent tests exact and fast. Plain C, no
 * ESP-IDF.
 */
#include "hal_timer.h"
#include "hal_timer_mock.h"

static uint32_t s_now_ms;

uint32_t hal_timer_get_ms(void)
{
    return s_now_ms;
}

uint32_t hal_timer_get_us(void)
{
    return s_now_ms * 1000u; // simulated: µs derived from the ms counter
}

void hal_timer_delay_ms(uint32_t ms)
{
    s_now_ms += ms; // a delay deterministically moves simulated time forward
}

void hal_timer_mock_advance_ms(uint32_t ms)
{
    s_now_ms += ms;
}

void hal_timer_mock_reset(void)
{
    s_now_ms = 0;
}
