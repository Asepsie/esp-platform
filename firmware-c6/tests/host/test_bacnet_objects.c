// =============================================================================
// test_bacnet_objects.c — host tests for the present-value binding.
//
// Exercises bacnet_object_present_value() against the real sensor_state store
// (re-init'd per test). Covers the diagnostic path, the space-aggregated path,
// and the error cases — without bacnet-stack in the loop.
// =============================================================================
#include "unity.h"
#include "bacnet_objects.h"
#include "bacnet_object_map.h"
#include "sensor_state.h"

#include <string.h>

void setUp(void)    { TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_init()); }
void tearDown(void) {}

// NULL arguments are rejected.
static void test_null_args(void)
{
    float v = 0.0f;
    const bacnet_object_map_entry_t *e = bacnet_object_map_lookup(0);
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG,
                          bacnet_object_present_value(NULL, &v));
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG,
                          bacnet_object_present_value(e, NULL));
}

// Diagnostic objects resolve straight from the store (300=deadline, 303=nvs).
static void test_diagnostic_values(void)
{
    sensor_state_increment_deadline_miss();
    sensor_state_increment_deadline_miss();
    sensor_state_set_nvs_status(false, 42);

    float v = -1.0f;
    TEST_ASSERT_EQUAL_INT(
        ESP_OK, bacnet_object_present_value(bacnet_object_map_lookup(300), &v));
    TEST_ASSERT_EQUAL_FLOAT(2.0f, v);

    TEST_ASSERT_EQUAL_INT(
        ESP_OK, bacnet_object_present_value(bacnet_object_map_lookup(303), &v));
    TEST_ASSERT_EQUAL_FLOAT(42.0f, v);
}

// A space-aggregated row resolves once its space exists (empty space → 0).
static void test_space_value(void)
{
    space_t sp;
    memset(&sp, 0, sizeof(sp));
    strncpy(sp.id, "zone_a", sizeof(sp.id) - 1);
    sp.type = SPACE_TYPE_ZONE;
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_add_space(&sp));

    float v = -1.0f;
    const bacnet_object_map_entry_t *temp = bacnet_object_map_lookup(0);
    TEST_ASSERT_EQUAL_INT(ESP_OK, bacnet_object_present_value(temp, &v));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, v); // no equipment bound yet
}

// A space row whose space is not commissioned yet does not resolve.
static void test_space_value_uncommissioned(void)
{
    float v = 0.0f;
    const bacnet_object_map_entry_t *temp = bacnet_object_map_lookup(0);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, bacnet_object_present_value(temp, &v));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_null_args);
    RUN_TEST(test_diagnostic_values);
    RUN_TEST(test_space_value);
    RUN_TEST(test_space_value_uncommissioned);
    return UNITY_END();
}
