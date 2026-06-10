/**
 * @file control_loop.c
 * @brief Thermostat control loop implementation.
 *
 * Pure decision logic (@c control_loop_decide), source selection
 * (@c control_loop_run), and actuation/state-keeping. Relays are driven only
 * through the GPIO HAL; this file touches no ESP-IDF drivers and no store —
 * input gathering (sensor_state / zigbee_bridge) lives in control_task.c, so
 * the decision logic stays pure and host-testable.
 */
#include "control_loop.h"
#include "hal_gpio.h"

#include <string.h>

/** @brief Space this loop controls (used by control_loop_tick). */
static char s_space_id[ID_LEN];

/** @brief Last successfully applied relay state (hysteresis memory). */
static relay_state_t s_relays;

/* --- hysteresis helpers (symmetric ±deadband/2 around the setpoint) ------- */

static bool heat_decision(const control_recipe_t *r, float temp, bool prev)
{
    const float half = r->deadband * 0.5f;
    if (temp <= r->setpoint_heat - half) return true;   /* cold → call heat   */
    if (temp >= r->setpoint_heat + half) return false;  /* warm → stop heat   */
    return prev;                                        /* in band → hold     */
}

static bool cool_decision(const control_recipe_t *r, float temp, bool prev)
{
    const float half = r->deadband * 0.5f;
    if (temp >= r->setpoint_cool + half) return true;   /* hot → call cool    */
    if (temp <= r->setpoint_cool - half) return false;  /* cool → stop cool   */
    return prev;                                        /* in band → hold     */
}

relay_state_t control_loop_decide(const control_recipe_t *recipe,
                                  float temperature_c, bool dry_contact_active,
                                  relay_state_t prev)
{
    relay_state_t out = { .heat = false, .cool = false, .fan = false };

    if (recipe == NULL) {
        return out;
    }

    /* Safety interlock takes precedence over everything else. */
    if (recipe->dry_contact_lockout && dry_contact_active) {
        return out; /* all off */
    }

    switch (recipe->hvac_mode) {
    case HVAC_MODE_HEAT:
        out.heat = heat_decision(recipe, temperature_c, prev.heat);
        break;
    case HVAC_MODE_COOL:
        out.cool = cool_decision(recipe, temperature_c, prev.cool);
        break;
    case HVAC_MODE_AUTO:
        out.heat = heat_decision(recipe, temperature_c, prev.heat);
        out.cool = cool_decision(recipe, temperature_c, prev.cool);
        if (out.heat) {
            out.cool = false; /* never heat and cool simultaneously */
        }
        break;
    case HVAC_MODE_FAN_ONLY:
        out.fan = true;
        return out;
    case HVAC_MODE_OFF:
    case HVAC_MODE_DRY: /* dehumidify not implemented yet → all off */
    default:
        return out; /* all off */
    }

    /* Circulate air whenever a heat/cool call is active. */
    out.fan = out.heat || out.cool;
    return out;
}

static esp_err_t apply_relays(relay_state_t r)
{
    esp_err_t err = hal_gpio_set(HAL_GPIO_RELAY_HEAT, r.heat);
    if (err != ESP_OK) return err;
    err = hal_gpio_set(HAL_GPIO_RELAY_COOL, r.cool);
    if (err != ESP_OK) return err;
    return hal_gpio_set(HAL_GPIO_RELAY_FAN, r.fan);
}

esp_err_t control_loop_init(const char *space_id)
{
    memset(s_space_id, 0, sizeof(s_space_id));
    if (space_id != NULL) {
        strncpy(s_space_id, space_id, sizeof(s_space_id) - 1);
    }
    s_relays = (relay_state_t){ .heat = false, .cool = false, .fan = false };
    return apply_relays(s_relays); /* drive to safe (all off) default */
}

esp_err_t control_loop_run_once(const control_recipe_t *recipe,
                                float temperature_c, bool dry_contact_active)
{
    relay_state_t next =
        control_loop_decide(recipe, temperature_c, dry_contact_active, s_relays);
    esp_err_t err = apply_relays(next);
    if (err == ESP_OK) {
        s_relays = next; /* latch only on successful actuation */
    }
    return err;
}

esp_err_t control_loop_run(const control_recipe_t *recipe,
                           const control_inputs_t *in, bool *fault)
{
    if (recipe == NULL || in == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    float temperature_c;
    if (in->zigbee_valid) {
        temperature_c = in->zigbee_temp;     // primary: Zigbee/H2
    } else if (in->local_valid) {
        temperature_c = in->local_temp;      // fallback: onboard SHT40
    } else {
        // No temperature source: raise fault and HOLD the last relay state.
        if (fault != NULL) {
            *fault = true;
        }
        return apply_relays(s_relays);
    }

    if (fault != NULL) {
        *fault = false;
    }
    return control_loop_run_once(recipe, temperature_c, in->dry_contact_active);
}

const char *control_loop_space_id(void)
{
    return s_space_id;
}

relay_state_t control_loop_get_relays(void)
{
    return s_relays;
}
