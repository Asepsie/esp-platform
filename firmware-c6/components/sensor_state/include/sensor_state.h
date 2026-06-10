// =============================================================================
// sensor_state.h — central runtime state store (mutex-protected API).
//
// The runtime spine of the C6 firmware (data-model §7). All five model layers
// live in one mutex-protected struct (private to sensor_state.c). This API is
// the ONLY legal cross-task interface to that state (RT-04). Every call takes
// the store mutex internally.
//
// Layer 1 (devices/attributes) is written by the zigbee_bridge UART client as
// reports arrive from the H2. Aggregation (Layer 1→2→3) runs automatically
// after each attribute/online change, so reads of space data are always current.
// =============================================================================
#ifndef SENSOR_STATE_H
#define SENSOR_STATE_H

#include "platform_compat.h"   // esp_err_t
#include "data_model.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- Lifecycle ---------------------------------------------------------------
// Initialize (or reset) the store. Safe to call again to clear state (tests).
esp_err_t sensor_state_init(void);

// --- Topology configuration (from commissioning / NVS load) ------------------
// These populate Layer 2/3 so bindings can be resolved and spaces aggregated.
// (Not in the data-model API list, which assumes NVS-loaded topology; exposed
// here so commissioning/tests can build the topology programmatically.)
esp_err_t sensor_state_add_space(const space_t *space);
esp_err_t sensor_state_add_equipment(const equipment_t *equipment);

// --- Layer 1 writes (zigbee_bridge UART client → store) ----------------------
// Register/update a device by IEEE address.
esp_err_t sensor_state_register_device(const zb_device_t *device);
// Mark a known device online/offline (re-aggregates affected spaces).
esp_err_t sensor_state_set_device_online(const char *ieee, bool online);
// Update one attribute value (already in engineering units). Rejects unknown
// devices (ESP_ERR_NOT_FOUND), unmapped clusters (ESP_ERR_NOT_FOUND), and
// implausible values (ESP_ERR_INVALID_ARG). Re-aggregates on success.
esp_err_t sensor_state_update_attribute(const char *ieee, uint16_t cluster,
                                        uint16_t attr, float value);

// Number of registered Zigbee devices (diagnostic / test observability).
uint8_t sensor_state_get_device_count(void);

// --- Layer 3 reads (control loop, BACnet task, LVGL) -------------------------
// Copy a space (including freshly aggregated data) by id.
esp_err_t sensor_state_get_space(const char *space_id, space_t *out);
// Resolve a BACnet object instance to a float value. Currently handles the
// store-owned diagnostic instances (300/301/302); space/equipment instances are
// resolved by the bacnet component (which owns BACNET_OBJECT_MAP).
esp_err_t sensor_state_get_bacnet_value(uint32_t instance, float *out);

// --- Layer 4 (BACnet task / LVGL → control loop) -----------------------------
esp_err_t sensor_state_set_recipe(const control_recipe_t *recipe);
esp_err_t sensor_state_get_recipe(control_recipe_t *out);

// --- Diagnostics -------------------------------------------------------------
uint32_t sensor_state_get_deadline_misses(void);
void     sensor_state_increment_deadline_miss(void);

// Record NVS health, called once at boot after hal_nvs_init(). The commit
// count is exposed northbound as BACnet Analog Input instance 303 (flash wear);
// the recovery flag is readable via sensor_state_get_nvs_recovered() and is
// intended to drive a BMS alarm on a factory-reset event.
void     sensor_state_set_nvs_status(bool recovered, uint32_t write_count);

// True if NVS corruption recovery ran at boot (else false).
bool     sensor_state_get_nvs_recovered(void);

#ifdef __cplusplus
}
#endif

#endif // SENSOR_STATE_H
