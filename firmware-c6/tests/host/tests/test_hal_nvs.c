// =============================================================================
// test_hal_nvs.c — host unit tests for the NVS HAL (file-backed mock).
// =============================================================================
#include "unity.h"
#include "hal_nvs.h"
#include "hal_nvs_mock.h"

#include <string.h>

void setUp(void)
{
    hal_nvs_mock_reset();
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_nvs_init());
}
void tearDown(void) {}

// write/read cycle for a string value
static void test_str_write_read_cycle(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_nvs_set_str("wifi_ssid", "ESP-Lab"));

    char out[32] = {0};
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_nvs_get_str("wifi_ssid", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("ESP-Lab", out);
}

// missing key returns ESP_ERR_NOT_FOUND (str and blob)
static void test_missing_key_returns_error(void)
{
    char out[16];
    TEST_ASSERT_EQUAL_INT(ESP_ERR_NOT_FOUND,
        hal_nvs_get_str("does_not_exist", out, sizeof(out)));

    uint8_t blob[8];
    size_t n = 0;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_NOT_FOUND,
        hal_nvs_get_blob("nope", blob, sizeof(blob), &n));
}

// blob round-trip preserves bytes and length
static void test_blob_round_trip(void)
{
    const uint8_t in[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x42};
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_nvs_set_blob("cert", in, sizeof(in)));

    uint8_t out[16] = {0};
    size_t out_len = 0;
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_nvs_get_blob("cert", out, sizeof(out), &out_len));
    TEST_ASSERT_EQUAL_size_t(sizeof(in), out_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(in, out, sizeof(in));
}

// erase removes a key; reading it afterwards fails; erasing twice reports missing
static void test_erase_removes_key(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_nvs_set_str("tmp", "x"));
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_nvs_erase("tmp"));

    char out[8];
    TEST_ASSERT_EQUAL_INT(ESP_ERR_NOT_FOUND, hal_nvs_get_str("tmp", out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(ESP_ERR_NOT_FOUND, hal_nvs_erase("tmp"));
}

// argument validation
static void test_invalid_args(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, hal_nvs_set_str(NULL, "v"));
    char out[8];
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, hal_nvs_get_str("k", out, 0));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_str_write_read_cycle);
    RUN_TEST(test_missing_key_returns_error);
    RUN_TEST(test_blob_round_trip);
    RUN_TEST(test_erase_removes_key);
    RUN_TEST(test_invalid_args);
    return UNITY_END();
}
