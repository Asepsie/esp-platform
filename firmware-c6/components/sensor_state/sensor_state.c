// =============================================================================
// sensor_state.c — central runtime state store implementation (data-model §7).
//
// Owns the single thermostat_state_t instance and its mutex. All public calls
// lock the mutex, mutate/read, then unlock. Layer 1 writes trigger automatic
// re-aggregation of Layer 3 (spaces) so reads are always consistent.
// =============================================================================
#include "sensor_state.h"
#include "cluster_map.h"

#include <string.h>

// BACnet diagnostic instances owned by the store (data-model §6.2).
#define BACNET_INST_DEADLINE_MISSES  300
#define BACNET_INST_ZIGBEE_LQI       301
#define BACNET_INST_BATTERY_MIN      302

// Central runtime state — private to this translation unit. Access is API-only
// (RT-04), so the struct never needs to appear in a public header.
typedef struct {
    // Layer 1 — physical Zigbee state
    zb_device_t    devices[MAX_ZB_DEVICES];
    uint8_t        device_count;
    zb_attribute_t attributes[MAX_ZB_DEVICES][MAX_ZB_CLUSTERS];

    // Layer 2 — functional equipment
    equipment_t    equipment[MAX_EQUIPMENT];
    uint8_t        equipment_count;

    // Layer 3 — spatial topology
    space_t        spaces[MAX_SPACES];
    uint8_t        space_count;

    // Layer 4 — active control recipe
    control_recipe_t active_recipe;

    // Layer 5 — BACnet mirror state
    uint32_t       bacnet_cov_sequence;

    // Diagnostics
    uint32_t       rt_deadline_miss_count;
    uint8_t        zigbee_avg_lqi;
    uint8_t        battery_min_pct;

    // Metadata
    uint32_t       last_update_ms;
    platform_mutex_t mutex;
    bool           initialized;
} thermostat_state_t;

static thermostat_state_t s;

// --- internal helpers (assume the mutex is held) -----------------------------

static int find_device_index(const char *ieee)
{
    for (uint8_t i = 0; i < s.device_count; i++) {
        if (strncmp(s.devices[i].ieee_addr, ieee, IEEE_ADDR_STR_LEN) == 0) {
            return (int)i;
        }
    }
    return -1;
}

// Find the attribute slot for (cluster, attr) on device `di`, allocating the
// next free slot if it does not yet exist. Returns -1 if the device is full.
static int find_or_alloc_attr_slot(int di, uint16_t cluster, uint16_t attr)
{
    int free_slot = -1;
    for (int j = 0; j < MAX_ZB_CLUSTERS; j++) {
        const zb_attribute_t *a = &s.attributes[di][j];
        if (a->valid && a->cluster_id == cluster && a->attribute_id == attr) {
            return j;
        }
        if (!a->valid && free_slot < 0) {
            free_slot = j;
        }
    }
    return free_slot;
}

// Find a valid attribute value for (ieee, cluster, attr). Returns true + value.
static bool resolve_attribute(const char *ieee, uint16_t cluster, uint16_t attr,
                              float *out_f, bool *out_b)
{
    int di = find_device_index(ieee);
    if (di < 0) {
        return false;
    }
    for (int j = 0; j < MAX_ZB_CLUSTERS; j++) {
        const zb_attribute_t *a = &s.attributes[di][j];
        if (a->valid && a->cluster_id == cluster && a->attribute_id == attr) {
            if (out_f) *out_f = a->value_float;
            if (out_b) *out_b = a->value_bool;
            return true;
        }
    }
    return false;
}

static bool device_online(const char *ieee)
{
    int di = find_device_index(ieee);
    return (di >= 0) && s.devices[di].online;
}

// Recompute the aggregated data for one space from its equipment bindings.
static void aggregate_space(space_t *sp)
{
    float t_sum = 0, h_sum = 0, c_sum = 0;
    int   t_n = 0, h_n = 0, c_n = 0;
    bool  any_dry = false;
    uint8_t equip_count = 0, online_count = 0;

    for (uint8_t k = 0; k < s.equipment_count; k++) {
        const equipment_t *e = &s.equipment[k];
        if (!e->enabled || strncmp(e->space_id, sp->id, ID_LEN) != 0) {
            continue;
        }
        equip_count++;
        bool any_online = false;

        for (uint8_t b = 0; b < e->binding_count; b++) {
            const data_binding_t *bind = &e->bindings[b];
            float vf = 0; bool vb = false;
            bool have = resolve_attribute(bind->device_ieee, bind->cluster_id,
                                          bind->attribute_id, &vf, &vb);
            if (have) {
                switch (bind->category) {
                case DATA_CAT_TEMPERATURE: t_sum += vf; t_n++; break;
                case DATA_CAT_HUMIDITY:    h_sum += vf; h_n++; break;
                case DATA_CAT_CO2:         c_sum += vf; c_n++; break;
                case DATA_CAT_DRY_CONTACT: any_dry = any_dry || vb; break;
                default: break; // battery / occupancy / unknown: not aggregated
                }
            }
            if (device_online(bind->device_ieee)) {
                any_online = true;
            }
        }
        if (any_online) {
            online_count++;
        }
    }

    space_aggregated_data_t *agg = &sp->aggregated;
    agg->avg_temperature = (t_n > 0) ? (t_sum / (float)t_n) : 0.0f;
    agg->avg_humidity    = (h_n > 0) ? (h_sum / (float)h_n) : 0.0f;
    agg->avg_co2         = (c_n > 0) ? (c_sum / (float)c_n) : 0.0f;
    agg->any_dry_contact = any_dry;
    agg->equipment_count = equip_count;
    agg->online_count    = online_count;
    agg->last_updated_ms = platform_now_ms();
}

static void aggregate_all_spaces(void)
{
    for (uint8_t i = 0; i < s.space_count; i++) {
        aggregate_space(&s.spaces[i]);
    }
}

// Recompute the lowest battery percentage across all valid battery attributes.
static void recompute_battery_min(void)
{
    uint8_t min_pct = 0xFF;
    for (uint8_t di = 0; di < s.device_count; di++) {
        for (int j = 0; j < MAX_ZB_CLUSTERS; j++) {
            const zb_attribute_t *a = &s.attributes[di][j];
            if (a->valid &&
                cluster_map_category(a->cluster_id, a->attribute_id) == DATA_CAT_BATTERY) {
                uint8_t pct = (uint8_t)a->value_float;
                if (pct < min_pct) {
                    min_pct = pct;
                }
            }
        }
    }
    s.battery_min_pct = (min_pct == 0xFF) ? 0 : min_pct;
}

// --- lifecycle ---------------------------------------------------------------

esp_err_t sensor_state_init(void)
{
    platform_mutex_t keep = s.initialized ? s.mutex : NULL;
    memset(&s, 0, sizeof(s));
    s.mutex = keep ? keep : platform_mutex_create();
    if (s.mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    s.battery_min_pct = 0;
    s.initialized = true;
    return ESP_OK;
}

// --- topology configuration --------------------------------------------------

esp_err_t sensor_state_add_space(const space_t *space)
{
    if (space == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    platform_mutex_lock(s.mutex);
    // Replace if an entry with the same id exists.
    for (uint8_t i = 0; i < s.space_count; i++) {
        if (strncmp(s.spaces[i].id, space->id, ID_LEN) == 0) {
            s.spaces[i] = *space;
            aggregate_space(&s.spaces[i]);
            platform_mutex_unlock(s.mutex);
            return ESP_OK;
        }
    }
    if (s.space_count >= MAX_SPACES) {
        platform_mutex_unlock(s.mutex);
        return ESP_ERR_NO_MEM;
    }
    s.spaces[s.space_count] = *space;
    aggregate_space(&s.spaces[s.space_count]);
    s.space_count++;
    platform_mutex_unlock(s.mutex);
    return ESP_OK;
}

esp_err_t sensor_state_add_equipment(const equipment_t *equipment)
{
    if (equipment == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    platform_mutex_lock(s.mutex);
    for (uint8_t i = 0; i < s.equipment_count; i++) {
        if (strncmp(s.equipment[i].id, equipment->id, ID_LEN) == 0) {
            s.equipment[i] = *equipment;
            aggregate_all_spaces();
            platform_mutex_unlock(s.mutex);
            return ESP_OK;
        }
    }
    if (s.equipment_count >= MAX_EQUIPMENT) {
        platform_mutex_unlock(s.mutex);
        return ESP_ERR_NO_MEM;
    }
    s.equipment[s.equipment_count] = *equipment;
    s.equipment_count++;
    aggregate_all_spaces();
    platform_mutex_unlock(s.mutex);
    return ESP_OK;
}

// --- Layer 1 writes ----------------------------------------------------------

esp_err_t sensor_state_register_device(const zb_device_t *device)
{
    if (device == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    platform_mutex_lock(s.mutex);
    int di = find_device_index(device->ieee_addr);
    if (di >= 0) {
        s.devices[di] = *device;
        platform_mutex_unlock(s.mutex);
        return ESP_OK;
    }
    if (s.device_count >= MAX_ZB_DEVICES) {
        platform_mutex_unlock(s.mutex);
        return ESP_ERR_NO_MEM;
    }
    s.devices[s.device_count] = *device;
    s.device_count++;
    platform_mutex_unlock(s.mutex);
    return ESP_OK;
}

esp_err_t sensor_state_set_device_online(const char *ieee, bool online)
{
    if (ieee == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    platform_mutex_lock(s.mutex);
    int di = find_device_index(ieee);
    if (di < 0) {
        platform_mutex_unlock(s.mutex);
        return ESP_ERR_NOT_FOUND;
    }
    s.devices[di].online = online;
    s.devices[di].last_seen_ms = platform_now_ms();
    aggregate_all_spaces();
    platform_mutex_unlock(s.mutex);
    return ESP_OK;
}

esp_err_t sensor_state_update_attribute(const char *ieee, uint16_t cluster,
                                        uint16_t attr, float value)
{
    if (ieee == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    platform_mutex_lock(s.mutex);

    int di = find_device_index(ieee);
    if (di < 0) {
        platform_mutex_unlock(s.mutex);
        return ESP_ERR_NOT_FOUND;
    }

    const cluster_map_entry_t *cm = cluster_map_lookup(cluster, attr);
    if (cm == NULL) {
        platform_mutex_unlock(s.mutex);
        return ESP_ERR_NOT_FOUND; // unmapped (cluster, attribute)
    }

    if (value < cm->min_plausible || value > cm->max_plausible) {
        platform_mutex_unlock(s.mutex);
        return ESP_ERR_INVALID_ARG; // implausible — dropped
    }

    int slot = find_or_alloc_attr_slot(di, cluster, attr);
    if (slot < 0) {
        platform_mutex_unlock(s.mutex);
        return ESP_ERR_NO_MEM; // device attribute table full
    }

    zb_attribute_t *a = &s.attributes[di][slot];
    a->cluster_id      = cluster;
    a->attribute_id    = attr;
    a->value_float     = value;
    a->value_bool      = (value != 0.0f);
    a->last_updated_ms = platform_now_ms();
    a->valid           = true;

    s.devices[di].last_seen_ms = a->last_updated_ms;

    if (cm->category == DATA_CAT_BATTERY) {
        recompute_battery_min();
    }
    aggregate_all_spaces();
    s.last_update_ms = a->last_updated_ms;

    platform_mutex_unlock(s.mutex);
    return ESP_OK;
}

// --- Layer 3 reads -----------------------------------------------------------

esp_err_t sensor_state_get_space(const char *space_id, space_t *out)
{
    if (space_id == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    platform_mutex_lock(s.mutex);
    for (uint8_t i = 0; i < s.space_count; i++) {
        if (strncmp(s.spaces[i].id, space_id, ID_LEN) == 0) {
            *out = s.spaces[i];
            platform_mutex_unlock(s.mutex);
            return ESP_OK;
        }
    }
    platform_mutex_unlock(s.mutex);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t sensor_state_get_bacnet_value(uint32_t instance, float *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    platform_mutex_lock(s.mutex);
    esp_err_t ret = ESP_OK;
    switch (instance) {
    case BACNET_INST_DEADLINE_MISSES:
        *out = (float)s.rt_deadline_miss_count;
        break;
    case BACNET_INST_ZIGBEE_LQI:
        *out = (float)s.zigbee_avg_lqi;
        break;
    case BACNET_INST_BATTERY_MIN:
        *out = (float)s.battery_min_pct;
        break;
    default:
        ret = ESP_ERR_NOT_FOUND; // space/equipment instances: bacnet component
        break;
    }
    platform_mutex_unlock(s.mutex);
    return ret;
}

// --- Layer 4 -----------------------------------------------------------------

esp_err_t sensor_state_set_recipe(const control_recipe_t *recipe)
{
    if (recipe == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    platform_mutex_lock(s.mutex);
    s.active_recipe = *recipe;
    platform_mutex_unlock(s.mutex);
    return ESP_OK;
}

esp_err_t sensor_state_get_recipe(control_recipe_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    platform_mutex_lock(s.mutex);
    *out = s.active_recipe;
    platform_mutex_unlock(s.mutex);
    return ESP_OK;
}

// --- Diagnostics -------------------------------------------------------------

uint32_t sensor_state_get_deadline_misses(void)
{
    platform_mutex_lock(s.mutex);
    uint32_t v = s.rt_deadline_miss_count;
    platform_mutex_unlock(s.mutex);
    return v;
}

void sensor_state_increment_deadline_miss(void)
{
    platform_mutex_lock(s.mutex);
    s.rt_deadline_miss_count++;
    platform_mutex_unlock(s.mutex);
}
