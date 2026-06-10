// =============================================================================
// test_control_loop.c — host unit tests for the control loop.
//
// Drives the control logic and asserts relay outputs through the GPIO HAL MOCK
// (hal_gpio_mock.c) — no hardware, no ESP-IDF. Setpoints: heat 21°C, cool 24°C,
// deadband 1.0°C → heat band [20.5, 21.5], cool band [23.5, 24.5].
// =============================================================================
#include "unity.h"
#include "control_loop.h"
#include "hal_gpio.h"
#include "hal_gpio_mock.h"

#include <string.h>

void setUp(void)
{
    hal_gpio_mock_reset_all();
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_gpio_init());
    TEST_ASSERT_EQUAL_INT(ESP_OK, control_loop_init("zone_a"));
}
void tearDown(void) {}

static control_recipe_t make_recipe(hvac_mode_t mode)
{
    control_recipe_t r;
    memset(&r, 0, sizeof(r));
    r.hvac_mode = mode;
    r.occupancy_mode = OCC_MODE_OCCUPIED;
    r.setpoint_heat = 21.0f;
    r.setpoint_cool = 24.0f;
    r.deadband = 1.0f;
    r.dry_contact_lockout = false;
    return r;
}

// Read a relay's current level through the HAL.
static bool relay(hal_gpio_id_t id)
{
    bool level = false;
    TEST_ASSERT_EQUAL_INT(ESP_OK, hal_gpio_get(id, &level));
    return level;
}

// temp below heat setpoint → RELAY_HEAT on
static void test_temp_below_heat_setpoint_relay_heat_on(void)
{
    control_recipe_t r = make_recipe(HVAC_MODE_HEAT);
    TEST_ASSERT_EQUAL_INT(ESP_OK, control_loop_run_once(&r, 18.0f, false));

    TEST_ASSERT_TRUE(relay(HAL_GPIO_RELAY_HEAT));
    TEST_ASSERT_FALSE(relay(HAL_GPIO_RELAY_COOL));
}

// temp above cool setpoint → RELAY_COOL on
static void test_temp_above_cool_setpoint_relay_cool_on(void)
{
    control_recipe_t r = make_recipe(HVAC_MODE_COOL);
    TEST_ASSERT_EQUAL_INT(ESP_OK, control_loop_run_once(&r, 27.0f, false));

    TEST_ASSERT_TRUE(relay(HAL_GPIO_RELAY_COOL));
    TEST_ASSERT_FALSE(relay(HAL_GPIO_RELAY_HEAT));
}

// temp in deadband → no relay change (holds previous state)
static void test_temp_in_deadband_no_relay_change(void)
{
    control_recipe_t r = make_recipe(HVAC_MODE_HEAT);

    // Cold: heat turns on.
    TEST_ASSERT_EQUAL_INT(ESP_OK, control_loop_run_once(&r, 18.0f, false));
    TEST_ASSERT_TRUE(relay(HAL_GPIO_RELAY_HEAT));

    // 21.0°C is inside the heat band [20.5, 21.5] → heat must stay ON.
    TEST_ASSERT_EQUAL_INT(ESP_OK, control_loop_run_once(&r, 21.0f, false));
    TEST_ASSERT_TRUE(relay(HAL_GPIO_RELAY_HEAT));

    // Conversely, starting from OFF and entering the band holds OFF.
    TEST_ASSERT_EQUAL_INT(ESP_OK, control_loop_init("zone_a")); // reset to off
    TEST_ASSERT_EQUAL_INT(ESP_OK, control_loop_run_once(&r, 21.0f, false));
    TEST_ASSERT_FALSE(relay(HAL_GPIO_RELAY_HEAT));
}

// mode OFF → all relays off (even when previously heating)
static void test_mode_off_all_relays_off(void)
{
    control_recipe_t heat = make_recipe(HVAC_MODE_HEAT);
    TEST_ASSERT_EQUAL_INT(ESP_OK, control_loop_run_once(&heat, 18.0f, false));
    TEST_ASSERT_TRUE(relay(HAL_GPIO_RELAY_HEAT)); // heating now

    control_recipe_t off = make_recipe(HVAC_MODE_OFF);
    TEST_ASSERT_EQUAL_INT(ESP_OK, control_loop_run_once(&off, 18.0f, false));

    TEST_ASSERT_FALSE(relay(HAL_GPIO_RELAY_HEAT));
    TEST_ASSERT_FALSE(relay(HAL_GPIO_RELAY_COOL));
    TEST_ASSERT_FALSE(relay(HAL_GPIO_RELAY_FAN));
}

// dry contact lockout → all relays off (interlock overrides a heat call)
static void test_dry_contact_lockout_all_relays_off(void)
{
    control_recipe_t r = make_recipe(HVAC_MODE_HEAT);
    r.dry_contact_lockout = true;

    // Contact inactive: cold temp would heat normally.
    TEST_ASSERT_EQUAL_INT(ESP_OK, control_loop_run_once(&r, 18.0f, false));
    TEST_ASSERT_TRUE(relay(HAL_GPIO_RELAY_HEAT));

    // Contact active: interlock forces everything off.
    TEST_ASSERT_EQUAL_INT(ESP_OK, control_loop_run_once(&r, 18.0f, true));
    TEST_ASSERT_FALSE(relay(HAL_GPIO_RELAY_HEAT));
    TEST_ASSERT_FALSE(relay(HAL_GPIO_RELAY_COOL));
    TEST_ASSERT_FALSE(relay(HAL_GPIO_RELAY_FAN));
}

// --- source-selection fallback (Zigbee primary → local SHT40 → fault) --------

// H2 online + valid Zigbee temp → uses Zigbee, ignores local.
static void test_control_uses_zigbee_temp_when_h2_online(void)
{
    control_recipe_t r = make_recipe(HVAC_MODE_HEAT); // heat setpoint 21, db 1
    control_inputs_t in = {
        .zigbee_temp = 18.0f, .zigbee_valid = true,   // cold via Zigbee → heat
        .local_temp = 25.0f,  .local_valid = true,    // warm local would NOT heat
        .dry_contact_active = false,
    };
    bool fault = true;
    TEST_ASSERT_EQUAL_INT(ESP_OK, control_loop_run(&r, &in, &fault));
    TEST_ASSERT_FALSE(fault);
    TEST_ASSERT_TRUE(relay(HAL_GPIO_RELAY_HEAT)); // proves Zigbee (18) was used
}

// H2 offline → falls back to the local SHT40 temperature.
static void test_control_falls_back_to_local_when_h2_offline(void)
{
    control_recipe_t r = make_recipe(HVAC_MODE_HEAT);
    control_inputs_t in = {
        .zigbee_temp = 25.0f, .zigbee_valid = false,  // H2 offline → ignore
        .local_temp = 18.0f,  .local_valid = true,    // cold local → heat
        .dry_contact_active = false,
    };
    bool fault = true;
    TEST_ASSERT_EQUAL_INT(ESP_OK, control_loop_run(&r, &in, &fault));
    TEST_ASSERT_FALSE(fault);
    TEST_ASSERT_TRUE(relay(HAL_GPIO_RELAY_HEAT)); // proves local (18) was used
}

// Neither source available → fault, and relays hold their previous state.
static void test_control_faults_when_both_unavailable(void)
{
    control_recipe_t r = make_recipe(HVAC_MODE_HEAT);

    // Establish a known state: heating on, from a valid run.
    control_inputs_t warm = {
        .zigbee_temp = 18.0f, .zigbee_valid = true,
        .local_valid = false, .dry_contact_active = false,
    };
    TEST_ASSERT_EQUAL_INT(ESP_OK, control_loop_run(&r, &warm, NULL));
    TEST_ASSERT_TRUE(relay(HAL_GPIO_RELAY_HEAT));

    // Now both sources gone → fault, relays must HOLD (stay on).
    control_inputs_t none = {
        .zigbee_valid = false, .local_valid = false, .dry_contact_active = false,
    };
    bool fault = false;
    TEST_ASSERT_EQUAL_INT(ESP_OK, control_loop_run(&r, &none, &fault));
    TEST_ASSERT_TRUE(fault);
    TEST_ASSERT_TRUE(relay(HAL_GPIO_RELAY_HEAT)); // unchanged (held)
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_temp_below_heat_setpoint_relay_heat_on);
    RUN_TEST(test_temp_above_cool_setpoint_relay_cool_on);
    RUN_TEST(test_temp_in_deadband_no_relay_change);
    RUN_TEST(test_mode_off_all_relays_off);
    RUN_TEST(test_dry_contact_lockout_all_relays_off);
    RUN_TEST(test_control_uses_zigbee_temp_when_h2_online);
    RUN_TEST(test_control_falls_back_to_local_when_h2_offline);
    RUN_TEST(test_control_faults_when_both_unavailable);
    return UNITY_END();
}
