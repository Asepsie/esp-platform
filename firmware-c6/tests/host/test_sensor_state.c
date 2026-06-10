// =============================================================================
// test_sensor_state.c — host unit tests for the central state store.
//
// Exercises Layer 1 writes, plausibility/validation, automatic Layer 3
// aggregation, recipe get/set, and diagnostics. The store is a singleton, so
// setUp() re-inits it to start each test from a clean slate.
// =============================================================================
#include "unity.h"
#include "sensor_state.h"

#include <string.h>

#define IEEE_A "0xAAAA000000000001"
#define IEEE_B "0xBBBB000000000002"
#define ZONE   "zone_a"

void setUp(void)    { TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_init()); }
void tearDown(void) {}

// --- small builders ----------------------------------------------------------

static zb_device_t make_device(const char *ieee, bool online)
{
    zb_device_t d;
    memset(&d, 0, sizeof(d));
    strncpy(d.ieee_addr, ieee, sizeof(d.ieee_addr) - 1);
    d.online = online;
    return d;
}

static space_t make_space(const char *id)
{
    space_t sp;
    memset(&sp, 0, sizeof(sp));
    strncpy(sp.id, id, sizeof(sp.id) - 1);
    sp.type = SPACE_TYPE_ZONE;
    return sp;
}

static equipment_t make_equipment(const char *id, const char *space_id)
{
    equipment_t e;
    memset(&e, 0, sizeof(e));
    strncpy(e.id, id, sizeof(e.id) - 1);
    strncpy(e.space_id, space_id, sizeof(e.space_id) - 1);
    e.enabled = true;
    return e;
}

static void add_binding(equipment_t *e, data_category_t cat, const char *ieee,
                        uint16_t cluster, uint16_t attr)
{
    data_binding_t *b = &e->bindings[e->binding_count++];
    memset(b, 0, sizeof(*b));
    b->category = cat;
    strncpy(b->device_ieee, ieee, sizeof(b->device_ieee) - 1);
    b->cluster_id = cluster;
    b->attribute_id = attr;
}

// Register device A (online), a zone, and an equipment in that zone with a
// single temperature binding to A. Used by several tests.
static void setup_single_temp(void)
{
    zb_device_t d = make_device(IEEE_A, true);
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_register_device(&d));

    space_t sp = make_space(ZONE);
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_add_space(&sp));

    equipment_t e = make_equipment("eq1", ZONE);
    add_binding(&e, DATA_CAT_TEMPERATURE, IEEE_A, 0x0402, 0x0000);
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_add_equipment(&e));
}

// --- tests -------------------------------------------------------------------

static void test_init_clears_state(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, sensor_state_get_deadline_misses());
    space_t out;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_NOT_FOUND, sensor_state_get_space(ZONE, &out));
}

static void test_update_attribute_aggregates_into_space(void)
{
    setup_single_temp();

    TEST_ASSERT_EQUAL_INT(ESP_OK,
        sensor_state_update_attribute(IEEE_A, 0x0402, 0x0000, 21.5f));

    space_t out;
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_get_space(ZONE, &out));
    TEST_ASSERT_EQUAL_FLOAT(21.5f, out.aggregated.avg_temperature);
    TEST_ASSERT_EQUAL_UINT8(1, out.aggregated.equipment_count);
    TEST_ASSERT_EQUAL_UINT8(1, out.aggregated.online_count); // A is online
}

static void test_update_unknown_device_rejected(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_ERR_NOT_FOUND,
        sensor_state_update_attribute("0xDEAD000000000000", 0x0402, 0x0000, 21.0f));
}

static void test_update_unmapped_cluster_rejected(void)
{
    zb_device_t d = make_device(IEEE_A, true);
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_register_device(&d));
    // 0x1234/0x0000 is not in the cluster map.
    TEST_ASSERT_EQUAL_INT(ESP_ERR_NOT_FOUND,
        sensor_state_update_attribute(IEEE_A, 0x1234, 0x0000, 1.0f));
}

static void test_update_implausible_value_rejected(void)
{
    zb_device_t d = make_device(IEEE_A, true);
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_register_device(&d));
    // Temperature plausibility is [-40, 80]; 999 is dropped.
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG,
        sensor_state_update_attribute(IEEE_A, 0x0402, 0x0000, 999.0f));
}

static void test_two_bindings_average(void)
{
    zb_device_t a = make_device(IEEE_A, true);
    zb_device_t b = make_device(IEEE_B, true);
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_register_device(&a));
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_register_device(&b));

    space_t sp = make_space(ZONE);
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_add_space(&sp));

    equipment_t e = make_equipment("eq1", ZONE);
    add_binding(&e, DATA_CAT_TEMPERATURE, IEEE_A, 0x0402, 0x0000);
    add_binding(&e, DATA_CAT_TEMPERATURE, IEEE_B, 0x0402, 0x0000);
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_add_equipment(&e));

    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_update_attribute(IEEE_A, 0x0402, 0x0000, 20.0f));
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_update_attribute(IEEE_B, 0x0402, 0x0000, 24.0f));

    space_t out;
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_get_space(ZONE, &out));
    TEST_ASSERT_EQUAL_FLOAT(22.0f, out.aggregated.avg_temperature); // (20+24)/2
}

static void test_dry_contact_any(void)
{
    zb_device_t d = make_device(IEEE_A, true);
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_register_device(&d));
    space_t sp = make_space(ZONE);
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_add_space(&sp));
    equipment_t e = make_equipment("eq1", ZONE);
    add_binding(&e, DATA_CAT_DRY_CONTACT, IEEE_A, 0x000F, 0x0055);
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_add_equipment(&e));

    space_t out;
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_update_attribute(IEEE_A, 0x000F, 0x0055, 1.0f));
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_get_space(ZONE, &out));
    TEST_ASSERT_TRUE(out.aggregated.any_dry_contact);
}

static void test_device_online_count_tracks_state(void)
{
    zb_device_t d = make_device(IEEE_A, false); // start offline
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_register_device(&d));
    space_t sp = make_space(ZONE);
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_add_space(&sp));
    equipment_t e = make_equipment("eq1", ZONE);
    add_binding(&e, DATA_CAT_TEMPERATURE, IEEE_A, 0x0402, 0x0000);
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_add_equipment(&e));

    space_t out;
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_get_space(ZONE, &out));
    TEST_ASSERT_EQUAL_UINT8(0, out.aggregated.online_count);

    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_set_device_online(IEEE_A, true));
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_get_space(ZONE, &out));
    TEST_ASSERT_EQUAL_UINT8(1, out.aggregated.online_count);
}

static void test_set_device_online_unknown_rejected(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_ERR_NOT_FOUND,
        sensor_state_set_device_online("0xNOPE", true));
}

static void test_recipe_roundtrip(void)
{
    control_recipe_t in;
    memset(&in, 0, sizeof(in));
    strncpy(in.id, "r1", sizeof(in.id) - 1);
    strncpy(in.space_id, ZONE, sizeof(in.space_id) - 1);
    in.hvac_mode = HVAC_MODE_HEAT;
    in.occupancy_mode = OCC_MODE_OCCUPIED;
    in.setpoint_heat = 21.0f;
    in.setpoint_cool = 25.0f;
    in.deadband = 0.5f;

    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_set_recipe(&in));

    control_recipe_t out;
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_get_recipe(&out));
    TEST_ASSERT_EQUAL_INT(HVAC_MODE_HEAT, out.hvac_mode);
    TEST_ASSERT_EQUAL_FLOAT(21.0f, out.setpoint_heat);
    TEST_ASSERT_EQUAL_FLOAT(25.0f, out.setpoint_cool);
    TEST_ASSERT_EQUAL_STRING(ZONE, out.space_id);
}

static void test_deadline_miss_counter(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, sensor_state_get_deadline_misses());
    sensor_state_increment_deadline_miss();
    sensor_state_increment_deadline_miss();
    TEST_ASSERT_EQUAL_UINT32(2, sensor_state_get_deadline_misses());

    // Exposed northbound as BACnet diagnostic instance 300.
    float v = -1.0f;
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_get_bacnet_value(300, &v));
    TEST_ASSERT_EQUAL_FLOAT(2.0f, v);
}

static void test_battery_min_diagnostic(void)
{
    zb_device_t a = make_device(IEEE_A, true);
    zb_device_t b = make_device(IEEE_B, true);
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_register_device(&a));
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_register_device(&b));

    // Power config 0x0001/0x0021 battery percentage.
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_update_attribute(IEEE_A, 0x0001, 0x0021, 80.0f));
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_update_attribute(IEEE_B, 0x0001, 0x0021, 55.0f));

    float v = -1.0f;
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_get_bacnet_value(302, &v));
    TEST_ASSERT_EQUAL_FLOAT(55.0f, v); // lowest across devices
}

static void test_bacnet_unknown_instance_rejected(void)
{
    float v;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_NOT_FOUND, sensor_state_get_bacnet_value(999, &v));
}

// NVS health (set at boot from hal_nvs): write count → AI 303, recovery flag.
static void test_nvs_status_diagnostic(void)
{
    // Defaults after init: not recovered, zero writes.
    TEST_ASSERT_FALSE(sensor_state_get_nvs_recovered());
    float v = -1.0f;
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_get_bacnet_value(303, &v));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, v);

    // Record a boot-time recovery + commit count.
    sensor_state_set_nvs_status(true, 7);

    TEST_ASSERT_TRUE(sensor_state_get_nvs_recovered());
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_get_bacnet_value(303, &v));
    TEST_ASSERT_EQUAL_FLOAT(7.0f, v);
}

// Local (onboard SHT40) source: unavailable until first update, then round-trips.
static void test_local_sensor_roundtrip(void)
{
    float t = -1.0f, rh = -1.0f;
    bool avail = true;
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_get_local(&t, &rh, &avail));
    TEST_ASSERT_FALSE(avail); // none yet after init

    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_update_local(22.5f, 48.0f));
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_get_local(&t, &rh, &avail));
    TEST_ASSERT_TRUE(avail);
    TEST_ASSERT_EQUAL_FLOAT(22.5f, t);
    TEST_ASSERT_EQUAL_FLOAT(48.0f, rh);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_clears_state);
    RUN_TEST(test_local_sensor_roundtrip);
    RUN_TEST(test_update_attribute_aggregates_into_space);
    RUN_TEST(test_update_unknown_device_rejected);
    RUN_TEST(test_update_unmapped_cluster_rejected);
    RUN_TEST(test_update_implausible_value_rejected);
    RUN_TEST(test_two_bindings_average);
    RUN_TEST(test_dry_contact_any);
    RUN_TEST(test_device_online_count_tracks_state);
    RUN_TEST(test_set_device_online_unknown_rejected);
    RUN_TEST(test_recipe_roundtrip);
    RUN_TEST(test_deadline_miss_counter);
    RUN_TEST(test_battery_min_diagnostic);
    RUN_TEST(test_bacnet_unknown_instance_rejected);
    RUN_TEST(test_nvs_status_diagnostic);
    return UNITY_END();
}
