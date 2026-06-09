// =============================================================================
// test_h2_hal.c — host unit tests for the H2 bsp HAL mocks.
//
// Validates that the H2 HAL mocks (loopback UART + GPIO capture) compile and
// behave, so they are ready to back future uart_bridge integration tests.
// =============================================================================
#include "unity.h"
#include "hal_uart.h"
#include "hal_uart_mock.h"
#include "hal_gpio.h"
#include "hal_gpio_mock.h"

#include <string.h>

void setUp(void)
{
    hal_uart_mock_reset();
    hal_gpio_mock_reset_all();
}
void tearDown(void) {}

// hal_uart loopback: bytes written come back in order on read.
static void test_uart_loopback_round_trip(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_uart_init());

    const uint8_t tx[] = {0xAA, 0x01, 0x00, 0x12, 0x34};
    TEST_ASSERT_EQUAL_INT((int)sizeof(tx), hal_uart_write(tx, sizeof(tx)));
    TEST_ASSERT_EQUAL_size_t(sizeof(tx), hal_uart_mock_pending());

    uint8_t rx[8] = {0};
    int n = hal_uart_read(rx, sizeof(rx), 0);
    TEST_ASSERT_EQUAL_INT((int)sizeof(tx), n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(tx, rx, sizeof(tx));

    // FIFO now empty → read returns 0 (mock "timeout").
    TEST_ASSERT_EQUAL_INT(0, hal_uart_read(rx, sizeof(rx), 0));
}

// hal_gpio: status LED set is captured; bad id rejected.
static void test_gpio_status_led(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_gpio_init());
    TEST_ASSERT_FALSE(hal_gpio_mock_get_state(HAL_GPIO_STATUS_LED)); // off after init

    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_gpio_set(HAL_GPIO_STATUS_LED, true));
    TEST_ASSERT_TRUE(hal_gpio_mock_get_state(HAL_GPIO_STATUS_LED));

    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, hal_gpio_set(HAL_GPIO_COUNT, true));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_uart_loopback_round_trip);
    RUN_TEST(test_gpio_status_led);
    return UNITY_END();
}
