// =============================================================================
// cluster_map.c — the cluster map table + lookup (data-model §2.3).
// =============================================================================
#include "cluster_map.h"

// The table. One row per supported ZCL (cluster, attribute) pair.
// scale_factor is ZCL-raw → engineering units (applied by the producer of the
// value, i.e. the H2 coprocessor); min/max_plausible are in engineering units.
static const cluster_map_entry_t CLUSTER_MAP[] = {
    // ZCL 0x0402 Temperature Measurement, attr 0x0000 MeasuredValue (1/100 °C).
    { 0x0402, 0x0000, DATA_CAT_TEMPERATURE, "Zone-Temperature", 0.01f, -40.0f,   80.0f },
    // ZCL 0x0405 Relative Humidity, attr 0x0000 MeasuredValue (1/100 %).
    { 0x0405, 0x0000, DATA_CAT_HUMIDITY,    "Zone-Humidity",    0.01f,   0.0f,  100.0f },
    // ZCL 0x040D CO2 Concentration, attr 0x0000 (ppm).
    { 0x040D, 0x0000, DATA_CAT_CO2,         "Zone-CO2",         1.0f,    0.0f, 5000.0f },
    // ZCL 0x000F Binary Input (Basic), attr 0x0055 PresentValue (dry contact).
    { 0x000F, 0x0055, DATA_CAT_DRY_CONTACT, "Zone-DryContact",  1.0f,    0.0f,    1.0f },
    // ZCL 0x0001 Power Configuration, attr 0x0021 BatteryPercentage (2× %).
    { 0x0001, 0x0021, DATA_CAT_BATTERY,     NULL,               0.5f,    0.0f,  100.0f },
};
#define CLUSTER_MAP_SIZE (sizeof(CLUSTER_MAP) / sizeof(CLUSTER_MAP[0]))

const cluster_map_entry_t *cluster_map_lookup(uint16_t cluster_id,
                                              uint16_t attribute_id)
{
    for (size_t i = 0; i < CLUSTER_MAP_SIZE; i++) {
        if (CLUSTER_MAP[i].cluster_id == cluster_id &&
            CLUSTER_MAP[i].attribute_id == attribute_id) {
            return &CLUSTER_MAP[i];
        }
    }
    return NULL;
}

data_category_t cluster_map_category(uint16_t cluster_id, uint16_t attribute_id)
{
    const cluster_map_entry_t *e = cluster_map_lookup(cluster_id, attribute_id);
    return e ? e->category : DATA_CAT_UNKNOWN;
}

size_t cluster_map_size(void)
{
    return CLUSTER_MAP_SIZE;
}

const cluster_map_entry_t *cluster_map_get(size_t index)
{
    return (index < CLUSTER_MAP_SIZE) ? &CLUSTER_MAP[index] : NULL;
}
