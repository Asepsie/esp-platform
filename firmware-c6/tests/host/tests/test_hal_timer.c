// =============================================================================
// test_hal_timer.c — host unit tests for the timing HAL (simulated-clock mock).
// =============================================================================
#include "unity.h"
#include "hal_timer.h"
#include "hal_timer_mock.h"

void setUp(void)    { hal_timer_mock_reset(); }
void tearDown(void) {}

// get_ms() never goes backwards and moves forward as the clock advances.
static void test_get_ms_monotonic_increasing(void)
{
    uint32_t t0 = hal_timer_get_ms();
    hal_timer_mock_advance_ms(5);
    uint32_t t1 = hal_timer_get_ms();
    TEST_ASSERT_TRUE(t1 > t0);

    hal_timer_mock_advance_ms(0);            // advancing by 0 must not regress
    TEST_ASSERT_TRUE(hal_timer_get_ms() >= t1);
}

// delay_ms() deterministically advances the simulated clock by the amount asked.
static void test_delay_ms_advances_mock_time(void)
{
    uint32_t t0 = hal_timer_get_ms();
    hal_timer_delay_ms(100);
    TEST_ASSERT_EQUAL_UINT32(t0 + 100, hal_timer_get_ms());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_get_ms_monotonic_increasing);
    RUN_TEST(test_delay_ms_advances_mock_time);
    return UNITY_END();
}
