// =============================================================================
// test_zigbee_bridge.c — end-to-end C6 bridge data path (host, no hardware).
//
// H2 sensor report → UART frame → bridge decode → cluster map → state store →
// space aggregation. Frames are injected via the mock (bypassing real UART);
// the rest of the path is the real production code.
// =============================================================================
#include "unity.h"
#include "zigbee_bridge.h"
#include "zigbee_bridge_mock.h"
#include "uart_bridge_protocol.h"
#include "sensor_state.h"
#include "hal_timer_mock.h"

#include <string.h>

#define DEV_IEEE_STR "0x00158d0001234567"
static const uint8_t DEV_IEEE[8] = {0x00, 0x15, 0x8d, 0x00, 0x01, 0x23, 0x45, 0x67};

void setUp(void)
{
    hal_timer_mock_reset();
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_init());
    TEST_ASSERT_EQUAL_INT(ESP_OK, zigbee_bridge_init()); // resets bridge state
}
void tearDown(void) {}

// --- builders ----------------------------------------------------------------

static bridge_sensor_report_t make_temp_report(float temp_c)
{
    bridge_sensor_report_t r;
    memset(&r, 0, sizeof(r));
    memcpy(r.ieee_addr, DEV_IEEE, 8);
    r.cluster_id   = 0x0402; // ZCL temperature
    r.attribute_id = 0x0000;
    r.data_type    = 0x29;
    r.value_float  = temp_c;
    r.lqi          = 200;
    r.battery_pct  = 95;
    return r;
}

// Register DEV_IEEE + a zone + an equipment with a temperature binding to it,
// so an incoming report aggregates into space "zone_a".
static void setup_temp_topology(void)
{
    zb_device_t d;
    memset(&d, 0, sizeof(d));
    strncpy(d.ieee_addr, DEV_IEEE_STR, sizeof(d.ieee_addr) - 1);
    d.online = true;
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_register_device(&d));

    space_t sp;
    memset(&sp, 0, sizeof(sp));
    strncpy(sp.id, "zone_a", sizeof(sp.id) - 1);
    sp.type = SPACE_TYPE_ZONE;
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_add_space(&sp));

    equipment_t e;
    memset(&e, 0, sizeof(e));
    strncpy(e.id, "eq1", sizeof(e.id) - 1);
    strncpy(e.space_id, "zone_a", sizeof(e.space_id) - 1);
    e.enabled = true;
    e.binding_count = 1;
    e.bindings[0].category = DATA_CAT_TEMPERATURE;
    strncpy(e.bindings[0].device_ieee, DEV_IEEE_STR, sizeof(e.bindings[0].device_ieee) - 1);
    e.bindings[0].cluster_id = 0x0402;
    e.bindings[0].attribute_id = 0x0000;
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_add_equipment(&e));
}

static void inject_heartbeat(void)
{
    const uint8_t seq[4] = {1, 0, 0, 0};
    uint8_t frame[BRIDGE_MAX_FRAME];
    int flen = bridge_frame_encode(MSG_HEARTBEAT, seq, sizeof(seq), frame, sizeof(frame));
    TEST_ASSERT_GREATER_THAN_INT(0, flen);
    zigbee_bridge_mock_inject_frame(frame, (size_t)flen);
}

// --- tests -------------------------------------------------------------------

// inject temp report → state store has the value (via aggregation)
static void test_sensor_report_updates_state_store(void)
{
    setup_temp_topology();
    bridge_sensor_report_t r = make_temp_report(21.5f);
    zigbee_bridge_mock_inject_sensor_report(&r);

    space_t out;
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_get_space("zone_a", &out));
    TEST_ASSERT_EQUAL_FLOAT(21.5f, out.aggregated.avg_temperature);
}

// inject DEVICE_JOIN → device registered in the store
static void test_device_join_registers_in_state(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, sensor_state_get_device_count());

    bridge_device_join_t d;
    memset(&d, 0, sizeof(d));
    memcpy(d.ieee_addr, DEV_IEEE, 8);
    d.short_addr = 0x1234;
    d.cluster_count = 1;
    d.supported_clusters[0] = 0x0402;
    strncpy(d.manufacturer, "Sonoff", sizeof(d.manufacturer) - 1);
    strncpy(d.model, "SNZB-02P", sizeof(d.model) - 1);

    uint8_t frame[BRIDGE_MAX_FRAME];
    int flen = bridge_frame_encode(MSG_DEVICE_JOIN, (const uint8_t *)&d,
                                   sizeof(d), frame, sizeof(frame));
    TEST_ASSERT_GREATER_THAN_INT(0, flen);
    zigbee_bridge_mock_inject_frame(frame, (size_t)flen);

    TEST_ASSERT_EQUAL_UINT8(1, sensor_state_get_device_count());
}

// inject heartbeat → H2 reported online
static void test_heartbeat_keeps_h2_online(void)
{
    TEST_ASSERT_FALSE(zigbee_bridge_is_h2_online()); // none received yet
    inject_heartbeat();
    TEST_ASSERT_TRUE(zigbee_bridge_is_h2_online());
}

// advance past the timeout with no further heartbeat → H2 offline
static void test_heartbeat_timeout_sets_h2_offline(void)
{
    inject_heartbeat();
    TEST_ASSERT_TRUE(zigbee_bridge_is_h2_online());

    hal_timer_mock_advance_ms(H2_HEARTBEAT_TIMEOUT_MS + 1000);
    TEST_ASSERT_FALSE(zigbee_bridge_is_h2_online());
}

// a frame with a corrupted CRC leaves the store unchanged
static void test_bad_crc_frame_rejected(void)
{
    setup_temp_topology();
    bridge_sensor_report_t r = make_temp_report(21.5f);

    uint8_t frame[BRIDGE_MAX_FRAME];
    int flen = bridge_frame_encode(MSG_SENSOR_REPORT, (const uint8_t *)&r,
                                   sizeof(r), frame, sizeof(frame));
    TEST_ASSERT_GREATER_THAN_INT(0, flen);
    frame[BRIDGE_OFF_PAYLOAD] ^= 0xFF; // corrupt → CRC mismatch

    zigbee_bridge_mock_inject_frame(frame, (size_t)flen);

    space_t out;
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_get_space("zone_a", &out));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, out.aggregated.avg_temperature); // unchanged
}

// a report drives Layer-3 space aggregation, not just raw storage
static void test_sensor_report_triggers_space_aggregation(void)
{
    setup_temp_topology();
    bridge_sensor_report_t r = make_temp_report(23.0f);
    zigbee_bridge_mock_inject_sensor_report(&r);

    space_t out;
    TEST_ASSERT_EQUAL_INT(ESP_OK, sensor_state_get_space("zone_a", &out));
    TEST_ASSERT_EQUAL_FLOAT(23.0f, out.aggregated.avg_temperature);
    TEST_ASSERT_EQUAL_UINT8(1, out.aggregated.equipment_count);
    TEST_ASSERT_EQUAL_UINT8(1, out.aggregated.online_count);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_sensor_report_updates_state_store);
    RUN_TEST(test_device_join_registers_in_state);
    RUN_TEST(test_heartbeat_keeps_h2_online);
    RUN_TEST(test_heartbeat_timeout_sets_h2_offline);
    RUN_TEST(test_bad_crc_frame_rejected);
    RUN_TEST(test_sensor_report_triggers_space_aggregation);
    return UNITY_END();
}
