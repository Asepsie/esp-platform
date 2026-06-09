// =============================================================================
// test_hal_wdt.c — host unit tests for the watchdog HAL (call-tracking mock).
// =============================================================================
#include "unity.h"
#include "hal_wdt.h"
#include "hal_wdt_mock.h"

void setUp(void)    { hal_wdt_mock_reset(); }
void tearDown(void) {}

// init + add_task register exactly once and succeed
static void test_task_registration(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_wdt_init());
    TEST_ASSERT_EQUAL_INT(1, hal_wdt_mock_init_count());

    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_wdt_add_task());
    TEST_ASSERT_EQUAL_INT(1, hal_wdt_mock_add_count());
    // A task that hasn't pet the dog yet has zero resets.
    TEST_ASSERT_EQUAL_INT(0, hal_wdt_mock_reset_count());
}

// reset is recorded each time it is called
static void test_reset_called(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_wdt_add_task());
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_INT(ESP_OK, hal_wdt_reset());
    }
    TEST_ASSERT_EQUAL_INT(3, hal_wdt_mock_reset_count());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_task_registration);
    RUN_TEST(test_reset_called);
    return UNITY_END();
}
