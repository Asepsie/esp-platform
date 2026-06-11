// =============================================================================
// test_cluster_handler.c — host unit tests for the Zigbee cluster handler.
//
// Exercises zigbee_cluster_handler_process(): ZCL data-type decode, raw ->
// engineering-unit scaling, plausibility rejection, the battery_pct side
// channel, LQI pass-through, and the is_supported() lookup.
//
// Pure host test: links ../../components/zigbee_coordinator/zigbee_cluster_handler.c.
// No ESP-IDF, no radio.
// =============================================================================
#include "unity.h"
#include "zigbee_cluster_handler.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

// A representative SNZB-02P-style IEEE address, reused across tests.
static const uint8_t SAMPLE_IEEE[8] = {0x00, 0x12, 0x4B, 0x00, 0x21, 0x5A, 0x3C, 0x7D};

// Temperature: ZCL 0x0402/0x0000, int16 (0x29), 1/100 °C. 2350 -> 23.50 °C.
static void test_temperature_report_converts_correctly(void)
{
    const uint8_t raw[2] = {0x2E, 0x09}; // 0x092E = 2350, little-endian
    bridge_sensor_report_t r;
    const esp_err_t err = zigbee_cluster_handler_process(
        SAMPLE_IEEE, 0x0402, 0x0000, 0x29, raw, sizeof(raw), 200, &r);

    TEST_ASSERT_EQUAL_INT(ESP_OK, err);
    TEST_ASSERT_EQUAL_HEX16(0x0402, r.cluster_id);
    TEST_ASSERT_EQUAL_HEX16(0x0000, r.attribute_id);
    TEST_ASSERT_EQUAL_HEX8(0x29, r.data_type);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 23.50f, r.value_float);
    TEST_ASSERT_FALSE(r.value_bool);
    TEST_ASSERT_EQUAL_UINT8(0xFF, r.battery_pct);       // not a battery report
    TEST_ASSERT_EQUAL_UINT8_ARRAY(SAMPLE_IEEE, r.ieee_addr, 8);
}

// Below the -40 °C floor -> rejected with INVALID_ARG, report left untouched.
static void test_temperature_below_plausibility_rejected(void)
{
    const uint8_t raw[2] = {0x78, 0xEC}; // 0xEC78 = -5000 -> -50.0 °C
    bridge_sensor_report_t r;
    memset(&r, 0xA5, sizeof(r));         // poison; must stay untouched on reject
    const esp_err_t err = zigbee_cluster_handler_process(
        SAMPLE_IEEE, 0x0402, 0x0000, 0x29, raw, sizeof(raw), 200, &r);

    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, err);
    TEST_ASSERT_EQUAL_HEX16(0xA5A5, r.cluster_id);      // unchanged
}

// Humidity: ZCL 0x0405/0x0000, uint16 (0x21), 1/100 %. 6500 -> 65.00 %.
static void test_humidity_report_converts_correctly(void)
{
    const uint8_t raw[2] = {0x64, 0x19}; // 0x1964 = 6500
    bridge_sensor_report_t r;
    const esp_err_t err = zigbee_cluster_handler_process(
        SAMPLE_IEEE, 0x0405, 0x0000, 0x21, raw, sizeof(raw), 150, &r);

    TEST_ASSERT_EQUAL_INT(ESP_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 65.00f, r.value_float);
}

// CO2: ZCL 0x040D/0x0000, uint16 (0x21), producer emits ppm (scale 1.0).
static void test_co2_report_converts_correctly(void)
{
    const uint8_t raw[2] = {0x20, 0x03}; // 0x0320 = 800 ppm
    bridge_sensor_report_t r;
    const esp_err_t err = zigbee_cluster_handler_process(
        SAMPLE_IEEE, 0x040D, 0x0000, 0x21, raw, sizeof(raw), 120, &r);

    TEST_ASSERT_EQUAL_INT(ESP_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 800.0f, r.value_float);
}

// Dry contact: ZCL 0x000F/0x0055, bool (0x10) -> value_bool true.
static void test_dry_contact_report_bool_true(void)
{
    const uint8_t raw[1] = {0x01};
    bridge_sensor_report_t r;
    const esp_err_t err = zigbee_cluster_handler_process(
        SAMPLE_IEEE, 0x000F, 0x0055, 0x10, raw, sizeof(raw), 90, &r);

    TEST_ASSERT_EQUAL_INT(ESP_OK, err);
    TEST_ASSERT_TRUE(r.value_bool);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, r.value_float);
}

// Battery: ZCL 0x0001/0x0021, uint8 (0x20), 2× %. 180 -> 90.0% in both fields.
static void test_battery_report_converts_correctly(void)
{
    const uint8_t raw[1] = {180};
    bridge_sensor_report_t r;
    const esp_err_t err = zigbee_cluster_handler_process(
        SAMPLE_IEEE, 0x0001, 0x0021, 0x20, raw, sizeof(raw), 200, &r);

    TEST_ASSERT_EQUAL_INT(ESP_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 90.0f, r.value_float);
    TEST_ASSERT_EQUAL_UINT8(90, r.battery_pct);
}

// An attribute we don't map (here: an unknown cluster) -> NOT_FOUND.
static void test_unknown_cluster_returns_not_found(void)
{
    const uint8_t raw[2] = {0x00, 0x00};
    bridge_sensor_report_t r;
    const esp_err_t err = zigbee_cluster_handler_process(
        SAMPLE_IEEE, 0x9999, 0x0000, 0x21, raw, sizeof(raw), 200, &r);

    TEST_ASSERT_EQUAL_INT(ESP_ERR_NOT_FOUND, err);
}

static void test_is_supported_returns_true_for_known(void)
{
    TEST_ASSERT_TRUE(zigbee_cluster_handler_is_supported(0x0402, 0x0000));  // temp
    TEST_ASSERT_TRUE(zigbee_cluster_handler_is_supported(0x0001, 0x0021));  // battery
}

static void test_is_supported_returns_false_for_unknown(void)
{
    TEST_ASSERT_FALSE(zigbee_cluster_handler_is_supported(0x0402, 0x0001)); // wrong attr
    TEST_ASSERT_FALSE(zigbee_cluster_handler_is_supported(0x9999, 0x0000)); // unmapped cluster
}

// LQI is copied verbatim into the report.
static void test_lqi_passed_through_to_report(void)
{
    const uint8_t raw[2] = {0x2E, 0x09};
    bridge_sensor_report_t r;
    const esp_err_t err = zigbee_cluster_handler_process(
        SAMPLE_IEEE, 0x0402, 0x0000, 0x29, raw, sizeof(raw), 173, &r);

    TEST_ASSERT_EQUAL_INT(ESP_OK, err);
    TEST_ASSERT_EQUAL_UINT8(173, r.lqi);
}

// --- Extra robustness (beyond the spec's named list) -------------------------

// Too few bytes for the declared type is a bad arg, not a silent misread.
static void test_truncated_payload_invalid_arg(void)
{
    const uint8_t raw[1] = {0x2E};      // int16 needs 2 bytes
    bridge_sensor_report_t r;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, zigbee_cluster_handler_process(
        SAMPLE_IEEE, 0x0402, 0x0000, 0x29, raw, sizeof(raw), 200, &r));
}

static void test_null_args_invalid_arg(void)
{
    const uint8_t raw[2] = {0x2E, 0x09};
    bridge_sensor_report_t r;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, zigbee_cluster_handler_process(
        NULL, 0x0402, 0x0000, 0x29, raw, sizeof(raw), 200, &r));
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, zigbee_cluster_handler_process(
        SAMPLE_IEEE, 0x0402, 0x0000, 0x29, NULL, sizeof(raw), 200, &r));
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, zigbee_cluster_handler_process(
        SAMPLE_IEEE, 0x0402, 0x0000, 0x29, raw, sizeof(raw), 200, NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_temperature_report_converts_correctly);
    RUN_TEST(test_temperature_below_plausibility_rejected);
    RUN_TEST(test_humidity_report_converts_correctly);
    RUN_TEST(test_co2_report_converts_correctly);
    RUN_TEST(test_dry_contact_report_bool_true);
    RUN_TEST(test_battery_report_converts_correctly);
    RUN_TEST(test_unknown_cluster_returns_not_found);
    RUN_TEST(test_is_supported_returns_true_for_known);
    RUN_TEST(test_is_supported_returns_false_for_unknown);
    RUN_TEST(test_lqi_passed_through_to_report);
    RUN_TEST(test_truncated_payload_invalid_arg);
    RUN_TEST(test_null_args_invalid_arg);
    return UNITY_END();
}
