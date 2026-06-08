// =============================================================================
// test_dummy.c — smoke test for the H2 host test harness.
//
// Proves the toolchain works end to end: Unity compiles, links, and ctest can
// run the resulting binary. It asserts nothing about the product — replace it
// with the real H2 suites (test_uart_bridge_framing, test_cluster_handler) as
// those components land, or keep it as a "harness is alive" canary.
//
// Anatomy of a Unity test file:
//   - setUp()/tearDown()  run before/after EVERY test (use for shared fixtures).
//   - each test is a void(void) function using TEST_ASSERT_* macros.
//   - main() lists the tests between UNITY_BEGIN() and UNITY_END().
//     UNITY_END() returns the failure count; returning it makes the process
//     exit non-zero on failure so ctest reports it.
//
// Useful for the upcoming framing tests:
//   TEST_ASSERT_EQUAL_HEX8(expected, actual)
//   TEST_ASSERT_EQUAL_HEX16(expected, actual)          // e.g. CRC16 checks
//   TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual, n) // e.g. frame bytes
// =============================================================================
#include "unity.h"

// Runs before each test. Put per-test setup here.
void setUp(void) {}

// Runs after each test. Put cleanup here.
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
