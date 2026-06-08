// =============================================================================
// test_dummy.c — smoke test for the C6 host test harness.
//
// Proves the toolchain works end to end: Unity compiles, links, and ctest can
// run the resulting binary. It asserts nothing about the product — delete it
// once real module tests exist, or keep it as a "harness is alive" canary.
//
// Anatomy of a Unity test file:
//   - setUp()/tearDown()  run before/after EVERY test (use for shared fixtures).
//   - each test is a void(void) function using TEST_ASSERT_* macros.
//   - main() lists the tests to run between UNITY_BEGIN() and UNITY_END().
//     UNITY_END() returns the number of failures, so returning it makes the
//     process exit non-zero on failure -> ctest reports the failure.
//
// Common assertions:
//   TEST_ASSERT_TRUE(x) / TEST_ASSERT_FALSE(x)
//   TEST_ASSERT_EQUAL_INT(expected, actual)
//   TEST_ASSERT_EQUAL_FLOAT(expected, actual)   // tolerance-based
//   TEST_ASSERT_EQUAL_STRING(expected, actual)
//   TEST_ASSERT_NULL(ptr) / TEST_ASSERT_NOT_NULL(ptr)
// =============================================================================
#include "unity.h"

// Runs before each test. Put per-test setup (init mocks, reset state) here.
void setUp(void) {}

// Runs after each test. Put cleanup (free, deinit) here.
void tearDown(void) {}

// A trivially-true check, just to exercise the harness.
static void test_harness_is_alive(void)
{
    TEST_ASSERT_EQUAL_INT(2, 1 + 1);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_harness_is_alive);
    return UNITY_END();
}
