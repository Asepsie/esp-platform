// =============================================================================
// bacnet_object_map.c — the BACnet object table + lookup (data-model Layer 5).
//
// One row per northbound object. Read-only sensor + diagnostic objects are
// live now (M0/M1); writable control objects (200–299, AV/BV/MSV/BO) are listed
// here as the design intent but are only instantiated once WriteProperty →
// set_recipe lands in M2 (bacnet_server.c filters by what each milestone backs).
//
// cov_increment is the analog COV threshold (engineering units) seeded into the
// stack's COV machinery in M3; cov_enabled gates participation.
// =============================================================================
#include "bacnet_object_map.h"

// Single zone for this SKU; matches control_loop_init("zone_a") in main.c.
#define ZONE_A "zone_a"

static const bacnet_object_map_entry_t BACNET_OBJECT_MAP[] = {
    // --- 0–99: space-aggregated (read-only, COV) --------------------------
    { 0, "Zone-Temperature", "Zone aggregated temperature",
      OBJ_ANALOG_INPUT, DATA_CAT_TEMPERATURE, ZONE_A, true, 0.5f },
    { 1, "Zone-Humidity", "Zone aggregated relative humidity",
      OBJ_ANALOG_INPUT, DATA_CAT_HUMIDITY, ZONE_A, true, 1.0f },
    { 2, "Zone-CO2", "Zone aggregated CO2",
      OBJ_ANALOG_INPUT, DATA_CAT_CO2, ZONE_A, true, 25.0f },
    { 10, "Zone-DryContact", "Zone any dry-contact closed",
      OBJ_BINARY_INPUT, DATA_CAT_DRY_CONTACT, ZONE_A, true, 0.0f },

    // --- 300–399: diagnostics (read-only) ---------------------------------
    // Resolved by sensor_state_get_bacnet_value(); no space/category binding.
    { 300, "Diag-RT-Deadline-Misses", "Control-loop deadline misses",
      OBJ_ANALOG_INPUT, DATA_CAT_UNKNOWN, "", false, 0.0f },
    { 301, "Diag-Zigbee-LQI", "Average Zigbee link quality",
      OBJ_ANALOG_INPUT, DATA_CAT_UNKNOWN, "", false, 0.0f },
    { 302, "Diag-Battery-Min", "Lowest sensor battery percent",
      OBJ_ANALOG_INPUT, DATA_CAT_UNKNOWN, "", false, 0.0f },
    { 303, "Diag-NVS-Writes", "NVS commit count (flash wear)",
      OBJ_ANALOG_INPUT, DATA_CAT_UNKNOWN, "", false, 0.0f },
};
#define BACNET_OBJECT_MAP_COUNT \
    (sizeof(BACNET_OBJECT_MAP) / sizeof(BACNET_OBJECT_MAP[0]))

size_t bacnet_object_map_size(void)
{
    return BACNET_OBJECT_MAP_COUNT;
}

const bacnet_object_map_entry_t *bacnet_object_map_get(size_t index)
{
    return (index < BACNET_OBJECT_MAP_COUNT) ? &BACNET_OBJECT_MAP[index] : NULL;
}

const bacnet_object_map_entry_t *bacnet_object_map_lookup(uint32_t instance)
{
    for (size_t i = 0; i < BACNET_OBJECT_MAP_COUNT; i++) {
        if (BACNET_OBJECT_MAP[i].instance == instance) {
            return &BACNET_OBJECT_MAP[i];
        }
    }
    return NULL;
}
