// =============================================================================
// test_io_scan.c — host unit tests for the wired-I/O scan.
//
// Built with IO_MCP23017_COUNT=IO_ADS1115_COUNT=IO_MCP4728_COUNT=1 (see the
// host CMake), so the expander phases are active. Drives the single-instance
// expander mock + the SHT40 mock; verifies the scan reads/writes and timing.
// =============================================================================
#include "unity.h"
#include "io_scan.h"
#include "hal_i2c_expander_mock.h"
#include "hal_sensor_local_mock.h"
#include "hal_timer_mock.h"
#include "sensor_state.h"

void setUp(void)
{
    hal_i2c_expander_mock_reset();
    hal_sensor_local_mock_reset();
    hal_timer_mock_reset();
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_init());
    TEST_ASSERT_EQUAL_INT(ESP_OK, io_scan_init());
}
void tearDown(void) {}

// DI: a tick reads the MCP23017 input port into the DI image.
static void test_di_read_returns_mocked_mcp23017_state(void)
{
    hal_mcp23017_mock_set_input(0, 0x05); // bits 0 and 2 set
    TEST_ASSERT_EQUAL_INT(ESP_OK, io_scan_tick());

    bool v;
    TEST_ASSERT_EQUAL_INT(ESP_OK, io_scan_get_di(0, &v)); TEST_ASSERT_TRUE(v);
    TEST_ASSERT_EQUAL_INT(ESP_OK, io_scan_get_di(1, &v)); TEST_ASSERT_FALSE(v);
    TEST_ASSERT_EQUAL_INT(ESP_OK, io_scan_get_di(2, &v)); TEST_ASSERT_TRUE(v);
}

// AI: a tick reads the previous conversion (channel 0, started at init).
static void test_ai_read_returns_mocked_ads1115_value(void)
{
    hal_ads1115_mock_set_raw(0, 16384); // half of full scale
    TEST_ASSERT_EQUAL_INT(ESP_OK, io_scan_tick());

    int16_t raw;
    TEST_ASSERT_EQUAL_INT(ESP_OK, io_scan_get_ai_raw(0, &raw));
    TEST_ASSERT_EQUAL_INT16(16384, raw);

    float v;
    TEST_ASSERT_EQUAL_INT(ESP_OK, io_scan_get_ai(0, &v));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.024f, v); // 16384/32768 * 2.048 V
}

// DO: a set value is written to the MCP23017 output port on the next tick.
static void test_do_write_propagates_to_mcp23017_mock(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_OK, io_scan_set_do(3, true));
    TEST_ASSERT_EQUAL_INT(ESP_OK, io_scan_tick());

    uint8_t out = hal_mcp23017_mock_get_output(1); // port B (outputs)
    TEST_ASSERT_TRUE((out >> 3) & 1u);
}

// AO: a set voltage is converted and written to the MCP4728 on the next tick.
static void test_ao_write_propagates_to_mcp4728_mock(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_OK, io_scan_set_ao(2, 1.65f)); // half of 3.3 V
    TEST_ASSERT_EQUAL_INT(ESP_OK, io_scan_tick());

    uint16_t code = hal_mcp4728_mock_get_channel(2);
    TEST_ASSERT_INT_WITHIN(2, 2047, code); // 1.65/3.3 * 4095
}

// Timing: a scan cycle stays well under the 100 ms budget.
static void test_scan_duration_under_100ms(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_OK, io_scan_tick());
    TEST_ASSERT_TRUE(io_scan_get_last_duration_us() < 100000u);
}

// SHT40 is read only every IO_SCAN_SHT40_INTERVAL-th tick (10).
static void test_sht40_read_only_every_10th_tick(void)
{
    hal_sensor_local_mock_set(22.0f, 50.0f);

    for (int i = 0; i < 9; i++) io_scan_tick();      // ticks 1..9
    TEST_ASSERT_EQUAL_UINT(0, hal_sensor_local_mock_read_count());

    io_scan_tick();                                  // tick 10 → read
    TEST_ASSERT_EQUAL_UINT(1, hal_sensor_local_mock_read_count());

    for (int i = 0; i < 10; i++) io_scan_tick();     // through tick 20 → read
    TEST_ASSERT_EQUAL_UINT(2, hal_sensor_local_mock_read_count());
}

// Safety: the interrupt path refreshes DI immediately, without a scan tick.
static void test_safety_interrupt_triggers_immediate_di_read(void)
{
    hal_mcp23017_mock_set_input(0, 0xFF);
    TEST_ASSERT_EQUAL_INT(ESP_OK, io_scan_safety_interrupt());

    bool v;
    TEST_ASSERT_EQUAL_INT(ESP_OK, io_scan_get_di(0, &v)); TEST_ASSERT_TRUE(v);
    TEST_ASSERT_EQUAL_INT(ESP_OK, io_scan_get_di(7, &v)); TEST_ASSERT_TRUE(v);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_di_read_returns_mocked_mcp23017_state);
    RUN_TEST(test_ai_read_returns_mocked_ads1115_value);
    RUN_TEST(test_do_write_propagates_to_mcp23017_mock);
    RUN_TEST(test_ao_write_propagates_to_mcp4728_mock);
    RUN_TEST(test_scan_duration_under_100ms);
    RUN_TEST(test_sht40_read_only_every_10th_tick);
    RUN_TEST(test_safety_interrupt_triggers_immediate_di_read);
    return UNITY_END();
}
