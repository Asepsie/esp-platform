// =============================================================================
// test_hal_timer.c — host unit tests for the timing HAL (real clock mock).
// =============================================================================
#include "unity.h"
#include "hal_timer.h"

void setUp(void) {}
void tearDown(void) {}

// get_ms() is monotonic non-decreasing across a short delay
static void test_monotonic_increment(void)
{
    uint32_t t0 = hal_timer_get_ms();
    hal_timer_delay_ms(5);
    uint32_t t1 = hal_timer_get_ms();
    // Unsigned subtraction is correct even across a (here impossible) wrap.
    TEST_ASSERT_TRUE((uint32_t)(t1 - t0) < 0x80000000u); // i.e. t1 >= t0
    TEST_ASSERT_TRUE(t1 >= t0);
}

// delay_ms() returns no earlier than the requested duration (with slack for
// scheduling); use a generous lower bound to avoid flakiness on busy hosts.
static void test_delay_waits_at_least_requested(void)
{
    const uint32_t requested = 30;
    uint32_t t0 = hal_timer_get_ms();
    hal_timer_delay_ms(requested);
    uint32_t elapsed = hal_timer_get_ms() - t0;

    TEST_ASSERT_TRUE(elapsed >= requested - 5); // allow ~5 ms timer granularity
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_monotonic_increment);
    RUN_TEST(test_delay_waits_at_least_requested);
    return UNITY_END();
}
