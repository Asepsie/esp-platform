// =============================================================================
// test_hal_wdt.c — host unit tests for the watchdog HAL (call-tracking mock).
// =============================================================================
#include "unity.h"
#include "hal_wdt.h"
#include "hal_wdt_mock.h"

void setUp(void)    { hal_wdt_mock_reset(); }
void tearDown(void) {}

// add_current_task() marks the task registered
static void test_wdt_task_registers(void)
{
    TEST_ASSERT_FALSE(hal_wdt_mock_is_task_registered());
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_wdt_add_current_task());
    TEST_ASSERT_TRUE(hal_wdt_mock_is_task_registered());
}

// reset() increments the tracked reset count
static void test_wdt_reset_increments_counter(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, hal_wdt_mock_get_reset_count());
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_INT(ESP_OK, hal_wdt_reset());
    }
    TEST_ASSERT_EQUAL_UINT32(3, hal_wdt_mock_get_reset_count());
}

// init(timeout_s) records the configured timeout
static void test_wdt_init_sets_timeout(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_wdt_init(8));
    TEST_ASSERT_EQUAL_UINT32(8, hal_wdt_mock_get_timeout_s());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_wdt_task_registers);
    RUN_TEST(test_wdt_reset_increments_counter);
    RUN_TEST(test_wdt_init_sets_timeout);
    return UNITY_END();
}
