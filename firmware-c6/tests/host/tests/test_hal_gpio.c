// =============================================================================
// test_hal_gpio.c — host unit tests for the GPIO HAL (via the host mock).
//
// Links hal_gpio_mock.c (not the target hal_gpio.c), so it builds with plain
// gcc and no ESP-IDF. Verifies the logical API and the mock inspection helpers.
// =============================================================================
#include "unity.h"
#include "hal_gpio.h"
#include "hal_gpio_mock.h"

// Fresh state before every test.
void setUp(void)    { hal_gpio_mock_reset_all(); }
void tearDown(void) {}

// Setting the heat relay high reads back high through the HAL API.
static void test_relay_heat_set_high_reads_high(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_gpio_init());

    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_gpio_set(HAL_GPIO_RELAY_HEAT, true));

    bool level = false;
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_gpio_get(HAL_GPIO_RELAY_HEAT, &level));
    TEST_ASSERT_TRUE(level);
    // And the captured mock state agrees.
    TEST_ASSERT_TRUE(hal_gpio_mock_get_state(HAL_GPIO_RELAY_HEAT));
}

// After init, the cool relay defaults to low (off).
static void test_relay_cool_default_is_low(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_gpio_init());

    bool level = true;
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_gpio_get(HAL_GPIO_RELAY_COOL, &level));
    TEST_ASSERT_FALSE(level);
}

// An out-of-range id is rejected by both set and get.
static void test_invalid_gpio_id_returns_error(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_gpio_init());

    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, hal_gpio_set(HAL_GPIO_COUNT, true));

    bool level;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, hal_gpio_get(HAL_GPIO_COUNT, &level));
    // NULL out-param is also invalid.
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, hal_gpio_get(HAL_GPIO_RELAY_FAN, NULL));
}

// reset_all() clears every captured line back to low.
static void test_mock_reset_clears_all_state(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_gpio_init());
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_gpio_set(HAL_GPIO_RELAY_HEAT, true));
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_gpio_set(HAL_GPIO_STATUS_LED, true));

    hal_gpio_mock_reset_all();

    for (int id = 0; id < HAL_GPIO_COUNT; id++) {
        TEST_ASSERT_FALSE(hal_gpio_mock_get_state((hal_gpio_id_t)id));
    }
    // After reset the init guard is cleared: set() fails until re-init.
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_STATE, hal_gpio_set(HAL_GPIO_RELAY_HEAT, true));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_relay_heat_set_high_reads_high);
    RUN_TEST(test_relay_cool_default_is_low);
    RUN_TEST(test_invalid_gpio_id_returns_error);
    RUN_TEST(test_mock_reset_clears_all_state);
    return UNITY_END();
}
