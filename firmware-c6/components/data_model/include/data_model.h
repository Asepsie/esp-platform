// =============================================================================
// data_model.h — five-layer thermostat data model (pure types).
//
// Implements the structs/enums from docs/architecture/data-model-v2.md:
//   Layer 1 Physical   — zb_device_t, zb_attribute_t, cluster_map_entry_t
//   Layer 2 Functional — data_binding_t, equipment_t (+ capability flags)
//   Layer 3 Topology   — space_t, space_aggregated_data_t
//   Layer 4 Orchestr.  — control_recipe_t (+ hvac/occupancy modes)
//   Layer 5 BMS        — bacnet_object_map_entry_t (+ object type enum)
//
// This header is intentionally dependency-free (no ESP-IDF, no FreeRTOS) so it
// compiles identically on target and in host unit tests. The runtime store that
// owns instances of these types (thermostat_state_t, which needs a mutex) is
// private to sensor_state.c — callers go through the sensor_state_*() API.
// =============================================================================
#ifndef DATA_MODEL_H
#define DATA_MODEL_H

#include <stdint.h>
#include <stdbool.h>

// Compile-time capacities (MAX_ZB_DEVICES, MAX_EQUIPMENT, MAX_SPACES, ...) are
// defined centrally in config/thermostat_config.h — the single source of truth.
#include "thermostat_config.h"

// Common string field sizes (kept as named constants for clarity/reuse).
#define IEEE_ADDR_STR_LEN   24   // e.g. "0x00158d0001234567"
#define FRIENDLY_NAME_LEN   48
#define ID_LEN              16
#define NAME_LEN            48
#define ALIAS_LEN           32
#define MFG_LEN             32
#define MODEL_LEN           32

// =============================================================================
// Layer 1 — Physical: Zigbee device + attribute model
// =============================================================================

// Physical Zigbee device descriptor (one per device, regardless of cluster count).
typedef struct {
    char     ieee_addr[IEEE_ADDR_STR_LEN];          // "0x00158d0001234567"
    char     friendly_name[FRIENDLY_NAME_LEN];      // user-assigned or auto
    uint16_t short_addr;                            // Zigbee network address (may change)
    uint8_t  endpoint;                              // primary endpoint
    uint16_t supported_clusters[MAX_ZB_CLUSTERS];   // discovered cluster IDs
    uint8_t  cluster_count;
    uint8_t  lqi;                                   // link quality 0–255
    bool     online;
    uint32_t last_seen_ms;
    char     manufacturer[MFG_LEN];                 // from Basic cluster 0x0000
    char     model[MODEL_LEN];
} zb_device_t;

// Raw (already normalized) Zigbee attribute value, indexed by (device, cluster/attr).
typedef struct {
    uint16_t cluster_id;       // ZCL cluster (e.g. 0x0402 = temperature)
    uint16_t attribute_id;     // ZCL attribute within cluster
    float    value_float;      // normalized engineering value (°C, %, ppm)
    bool     value_bool;       // for binary attributes (dry contact)
    uint8_t  data_type;        // ZCL data type tag (raw, for diagnostics)
    uint32_t last_updated_ms;
    bool     valid;            // false until first valid report received
} zb_attribute_t;

// Semantic category of a data point (drives binding, aggregation, BACnet mapping).
typedef enum {
    DATA_CAT_TEMPERATURE,   // °C
    DATA_CAT_HUMIDITY,      // % RH
    DATA_CAT_CO2,           // ppm
    DATA_CAT_OCCUPANCY,     // boolean (future)
    DATA_CAT_DRY_CONTACT,   // boolean
    DATA_CAT_BATTERY,       // % (internal use, not northbound)
    DATA_CAT_UNKNOWN,
} data_category_t;

// Cluster map entry — maps a (cluster, attribute) pair to semantics + bounds.
// The table itself lives in cluster_map.c; look up via cluster_map_lookup().
typedef struct {
    uint16_t         cluster_id;
    uint16_t         attribute_id;
    data_category_t  category;
    const char      *bacnet_object_name; // NULL if not northbound-exposed
    float            scale_factor;       // ZCL raw → engineering units
    float            min_plausible;      // values outside [min,max] are dropped
    float            max_plausible;
} cluster_map_entry_t;

// =============================================================================
// Layer 2 — Functional: Equipment model
// =============================================================================

// Binding from an equipment capability to a physical Zigbee attribute source.
typedef struct {
    data_category_t  category;                 // semantic role of this binding
    char             device_ieee[IEEE_ADDR_STR_LEN];
    uint16_t         cluster_id;
    uint16_t         attribute_id;
    char             alias[ALIAS_LEN];         // e.g. "primary-temp"
    bool             historize;                // log to NVS ring buffer (Tier 3)
} data_binding_t;

// Equipment capability bitmask (set bits = exposed data categories).
typedef enum {
    EQUIP_CAP_TEMPERATURE = (1 << 0),
    EQUIP_CAP_HUMIDITY    = (1 << 1),
    EQUIP_CAP_CO2         = (1 << 2),
    EQUIP_CAP_DRY_CONTACT = (1 << 3),
    EQUIP_CAP_HEAT        = (1 << 4),
    EQUIP_CAP_COOL        = (1 << 5),
    EQUIP_CAP_FAN         = (1 << 6),
} equipment_capability_t;

// Functional equipment unit — the BMS/UI-facing abstraction over devices.
typedef struct {
    char           id[ID_LEN];                              // short UUID
    char           name[NAME_LEN];                          // "Zone A Sensor Cluster"
    uint32_t       capabilities;                            // bitmask of equipment_capability_t
    data_binding_t bindings[MAX_BINDINGS_PER_EQUIPMENT];
    uint8_t        binding_count;
    char           space_id[ID_LEN];                        // FK → space_t.id
    bool           enabled;
} equipment_t;

// =============================================================================
// Layer 3 — Topology: Space model
// =============================================================================

typedef enum {
    SPACE_TYPE_BUILDING,  // whole building (root)
    SPACE_TYPE_FLOOR,     // floor level
    SPACE_TYPE_ZONE,      // HVAC zone — maps to a BACnet Zone object
    SPACE_TYPE_SPACE,     // individual room / open-plan area
} space_type_t;

// Space-level aggregated sensor data (computed; never written directly).
typedef struct {
    float    avg_temperature;   // °C  — AVG of temperature bindings
    float    avg_humidity;      // %   — AVG of humidity bindings
    float    avg_co2;           // ppm — AVG of co2 bindings
    bool     any_dry_contact;   // OR of all dry-contact bindings
    uint8_t  equipment_count;   // total equipment in this space
    uint8_t  online_count;      // equipment with at least one online device
    uint32_t last_updated_ms;
} space_aggregated_data_t;

// Spatial unit in the building topology.
typedef struct {
    char                    id[ID_LEN];
    char                    name[NAME_LEN];                       // "Floor 3 Zone A"
    space_type_t            type;
    char                    parent_id[ID_LEN];                    // "" = root
    char                    equipment_ids[MAX_EQUIPMENT_PER_SPACE][ID_LEN];
    uint8_t                 equipment_count;
    space_aggregated_data_t aggregated;
    uint32_t                bacnet_instance;
} space_t;

// =============================================================================
// Layer 4 — Orchestration: modes + control recipe
// =============================================================================

typedef enum {
    HVAC_MODE_OFF      = 0,
    HVAC_MODE_HEAT     = 1,
    HVAC_MODE_COOL     = 2,
    HVAC_MODE_AUTO     = 3,
    HVAC_MODE_FAN_ONLY = 4,
    HVAC_MODE_DRY      = 5,
} hvac_mode_t;

typedef enum {
    OCC_MODE_OCCUPIED   = 0,  // full comfort setpoints
    OCC_MODE_UNOCCUPIED = 1,  // reduced setpoints
    OCC_MODE_STANDBY    = 2,  // minimal conditioning
    OCC_MODE_SETBACK    = 3,  // energy-saving offset from comfort
} occupancy_mode_t;

// Active control recipe for a space (written via sensor_state_set_recipe only).
typedef struct {
    char             id[ID_LEN];
    char             space_id[ID_LEN];
    hvac_mode_t      hvac_mode;
    occupancy_mode_t occupancy_mode;
    float            setpoint_heat;       // °C
    float            setpoint_cool;       // °C
    float            setpoint_humidity;   // % (future)
    float            deadband;            // °C hysteresis
    bool             co2_override;        // force ventilation if CO2 > threshold
    float            co2_threshold;       // ppm
    bool             dry_contact_lockout; // disable output if contact open
} control_recipe_t;

// =============================================================================
// Layer 5 — BMS: BACnet object map (table owned by the bacnet component)
// =============================================================================

typedef enum {
    OBJ_ANALOG_INPUT,
    OBJ_ANALOG_OUTPUT,
    OBJ_ANALOG_VALUE,
    OBJ_BINARY_INPUT,
    OBJ_BINARY_OUTPUT,
    OBJ_BINARY_VALUE,
    OBJ_MULTI_STATE_VALUE,
} bacnet_obj_type_t;

// Defines the full northbound identity of one BACnet object.
typedef struct {
    uint32_t          instance;
    const char       *object_name;
    const char       *description;
    bacnet_obj_type_t type;
    data_category_t   source_category;     // DATA_CAT_* driving the value, or -1
    char              source_space_id[ID_LEN];
    bool              cov_enabled;
    float             cov_increment;
} bacnet_object_map_entry_t;

#endif // DATA_MODEL_H
