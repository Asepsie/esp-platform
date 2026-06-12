// =============================================================================
// bacnet_objects.c — present-value binding (object map row → sensor_state).
//
// No bacnet-stack types here on purpose: this keeps the value-projection logic
// pure and host-testable. The stack-facing side (creating AI/BI objects and
// pushing these values in) lives in bacnet_server.c.
// =============================================================================
#include "bacnet_objects.h"

#include "sensor_state.h"

esp_err_t bacnet_object_present_value(const bacnet_object_map_entry_t *e,
                                      float *out)
{
    if (e == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // 1. Store-owned instances (diagnostics 300–303, and any future direct
    //    mappings). Returns ESP_ERR_NOT_FOUND for instances it does not own.
    esp_err_t err = sensor_state_get_bacnet_value(e->instance, out);
    if (err != ESP_ERR_NOT_FOUND) {
        return err; // ESP_OK or a real failure (timeout/invalid)
    }

    // 2. Space-aggregated rows: pull the field selected by category.
    if (e->source_space_id[0] == '\0') {
        return ESP_ERR_NOT_FOUND;
    }
    space_t space;
    err = sensor_state_get_space(e->source_space_id, &space);
    if (err != ESP_OK) {
        return err;
    }
    switch (e->source_category) {
    case DATA_CAT_TEMPERATURE:
        *out = space.aggregated.avg_temperature;
        return ESP_OK;
    case DATA_CAT_HUMIDITY:
        *out = space.aggregated.avg_humidity;
        return ESP_OK;
    case DATA_CAT_CO2:
        *out = space.aggregated.avg_co2;
        return ESP_OK;
    case DATA_CAT_DRY_CONTACT:
        *out = space.aggregated.any_dry_contact ? 1.0f : 0.0f;
        return ESP_OK;
    default:
        return ESP_ERR_INVALID_ARG; // category not projectable to a value
    }
}
