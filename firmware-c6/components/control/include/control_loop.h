/**
 * @file control_loop.h
 * @brief Thermostat control loop — relay decisions from recipe + temperature.
 *
 * The control loop turns the active @c control_recipe_t and a space's measured
 * temperature / dry-contact state into heat/cool/fan relay commands, driven
 * through the GPIO HAL (@c hal_gpio.h). It applies symmetric hysteresis of
 * ±deadband/2 around each setpoint so the relays hold their state inside the
 * deadband (no short-cycling).
 *
 * Decision precedence: dry-contact lockout → HVAC mode → hysteresis.
 *
 * @see docs/architecture/data-model-v2.md §5 (recipe), §9 (data flow).
 * @see docs/architecture/rt-rules-v2.md RT-01 (control_loop is the hard-RT task).
 */
#ifndef CONTROL_LOOP_H
#define CONTROL_LOOP_H

#include <stdbool.h>
#include "platform_compat.h"
#include "data_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Desired relay outputs for one control decision. */
typedef struct {
    bool heat; /**< Heat-call relay. */
    bool cool; /**< Cool-call relay. */
    bool fan;  /**< Fan relay. */
} relay_state_t;

/**
 * @brief Initialize the control loop for a given space.
 *
 * Records which space this loop controls (used by @c control_loop_tick), resets
 * the internal relay state to all-off, and drives the relays off. The GPIO HAL
 * must already be initialized (@c hal_gpio_init()).
 *
 * @param space_id Space id this loop controls (matches @c space_t.id). May be
 *                 NULL/empty if only @c control_loop_run_once() is used.
 * @return @c ESP_OK, or the first @c esp_err_t from driving the relays off.
 */
esp_err_t control_loop_init(const char *space_id);

/**
 * @brief Pure control decision — no hardware access.
 *
 * @param recipe              Active recipe (NULL ⇒ all relays off).
 * @param temperature_c       Current measured temperature (°C).
 * @param dry_contact_active  True if a dry-contact interlock is asserted.
 * @param prev                Previous relay state (for in-deadband hold).
 * @return The decided relay outputs.
 */
relay_state_t control_loop_decide(const control_recipe_t *recipe,
                                  float temperature_c, bool dry_contact_active,
                                  relay_state_t prev);

/**
 * @brief Run one decision against the given inputs and drive the relays.
 *
 * Threads the internal previous-state through @c control_loop_decide(), applies
 * the result to the GPIO HAL, and (on success) latches it as the new state.
 *
 * @param recipe              Active recipe (NULL ⇒ all relays off).
 * @param temperature_c       Current measured temperature (°C).
 * @param dry_contact_active  True if a dry-contact interlock is asserted.
 * @return @c ESP_OK, or the first @c esp_err_t from the GPIO HAL.
 */
esp_err_t control_loop_run_once(const control_recipe_t *recipe,
                                float temperature_c, bool dry_contact_active);

/**
 * @brief Periodic firmware entry point (RT-01 control_loop task, 1 Hz).
 *
 * Reads the controlled space's aggregated temperature + dry-contact and the
 * active recipe from @c sensor_state, then runs one decision. No-op if the
 * recipe or space cannot be read.
 */
void control_loop_tick(void);

/**
 * @brief Get the relay state latched by the last successful run.
 * @return Current commanded relay outputs.
 */
relay_state_t control_loop_get_relays(void);

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_LOOP_H */
