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
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_nvs_init(NULL));
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

// 3 rapid writes within the coalescing window produce exactly one commit
static void test_write_coalesce_single_commit(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, hal_nvs_get_write_count());

    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_nvs_set_str("a", "1"));
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_nvs_set_str("b", "2"));
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_nvs_set_str("c", "3"));
    // Still inside the 2 s window → nothing committed yet.
    TEST_ASSERT_EQUAL_UINT32(0, hal_nvs_get_write_count());

    hal_nvs_mock_advance_ms(2100); // idle past the window
    TEST_ASSERT_EQUAL_UINT32(1, hal_nvs_get_write_count());
}

// flush() commits immediately without waiting for the window
static void test_flush_forces_immediate_commit(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_nvs_set_str("k", "v"));
    TEST_ASSERT_EQUAL_UINT32(0, hal_nvs_get_write_count());

    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_nvs_flush());
    TEST_ASSERT_EQUAL_UINT32(1, hal_nvs_get_write_count());
}

// the counter increments once per commit; a no-op flush does not bump it
static void test_write_counter_increments_on_commit(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_nvs_set_str("k", "1"));
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_nvs_flush());
    TEST_ASSERT_EQUAL_UINT32(1, hal_nvs_get_write_count());

    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_nvs_set_str("k", "2"));
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_nvs_flush());
    TEST_ASSERT_EQUAL_UINT32(2, hal_nvs_get_write_count());

    // Nothing dirty → flush is a no-op, counter unchanged.
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_nvs_flush());
    TEST_ASSERT_EQUAL_UINT32(2, hal_nvs_get_write_count());
}

// init() recovers from simulated corruption: factory reset + recovered flag
static void test_corruption_recovery_reinitializes(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_nvs_set_str("keep", "x"));
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_nvs_flush());
    TEST_ASSERT_EQUAL_UINT32(1, hal_nvs_get_write_count());

    hal_nvs_mock_set_corrupt(true);
    bool recovered = false;
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_nvs_init(&recovered));
    TEST_ASSERT_TRUE(recovered);

    // Store wiped to factory defaults, counter reset.
    char out[8];
    TEST_ASSERT_EQUAL_INT(ESP_ERR_NOT_FOUND, hal_nvs_get_str("keep", out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT32(0, hal_nvs_get_write_count());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_str_write_read_cycle);
    RUN_TEST(test_missing_key_returns_error);
    RUN_TEST(test_blob_round_trip);
    RUN_TEST(test_erase_removes_key);
    RUN_TEST(test_invalid_args);
    RUN_TEST(test_write_coalesce_single_commit);
    RUN_TEST(test_flush_forces_immediate_commit);
    RUN_TEST(test_write_counter_increments_on_commit);
    RUN_TEST(test_corruption_recovery_reinitializes);
    return UNITY_END();
}
