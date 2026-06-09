> **v2.0 note:** Zigbee data (Layer 1) now arrives from ESP32-H2 via UART bridge.
> The cluster_map and data_binding layers are unchanged — only the transport differs.
> See uart-bridge-protocol.md for the H2→C6 message format.

# Data Model
> **Thermostat Firmware — Architecture Document**
> Version: 1.0 | Status: Authoritative
> Inspired by Sowel's Device/Equipment/Zone separation (https://docs.sowel.org/technical/data-model/).
> Extended upward (BACnet/SC northbound) and downward (Zigbee cluster fidelity)
> for commercial BMS/HVAC context.

---

## 1. Five-layer architecture

```
┌──────────────────────────────────────────────────────────────┐
│  LAYER 5 — BMS INTEGRATION                                    │
│  BACnet/SC object model — northbound protocol surface         │
│  Translates Space/Equipment semantics to BACnet objects       │
│  BMS operators see spaces and equipment, not data points      │
├──────────────────────────────────────────────────────────────┤
│  LAYER 4 — ORCHESTRATION                                      │
│  Modes · Schedules · Control Recipes                          │
│  occupied / unoccupied / standby / setback                    │
│  Applied per space — same pattern as Sowel RecipeInstance     │
├──────────────────────────────────────────────────────────────┤
│  LAYER 3 — TOPOLOGY (Spaces)                                  │
│  Building → Floor → Zone → Space hierarchy                   │
│  Aggregates equipment data automatically (avg/any)            │
│  BACnet Zone object maps 1:1 to a space                       │
├──────────────────────────────────────────────────────────────┤
│  LAYER 2 — FUNCTIONAL (Equipment)                             │
│  HVAC unit · sensor cluster · relay group                     │
│  Binds physical devices to semantic roles via data_binding_t  │
│  What the BMS operator and LVGL UI see                        │
├──────────────────────────────────────────────────────────────┤
│  LAYER 1 — PHYSICAL (Devices + Attributes)                    │
│  Zigbee device → clusters → attributes                        │
│  Raw, protocol-native. Never exposed directly northbound.     │
│  cluster_map_entry_t is the bridge to Layer 2                 │
└──────────────────────────────────────────────────────────────┘
```

**Core principle (from Sowel):** A Device is what's on the network. An Equipment is what's in the room.

---

## 2. Layer 1 — Physical: Zigbee device and attribute model

### 2.1 Device discovery record

Populated on Zigbee join. Persisted to NVS device table.

```c
// Max simultaneous Zigbee devices (compile-time constant)
#define MAX_ZB_DEVICES      8
#define MAX_ZB_CLUSTERS     8

/**
 * @brief Physical Zigbee device descriptor.
 *
 * Discovered on network join, persisted in NVS. One entry per
 * Zigbee device regardless of how many clusters it exposes.
 */
typedef struct {
    char     ieee_addr[24];                    ///< "0x00158d0001234567"
    char     friendly_name[48];               ///< User-assigned or auto-generated
    uint16_t short_addr;                       ///< Zigbee network address (may change)
    uint8_t  endpoint;                         ///< Primary endpoint
    uint16_t supported_clusters[MAX_ZB_CLUSTERS]; ///< Discovered cluster IDs
    uint8_t  cluster_count;
    uint8_t  lqi;                              ///< Link quality 0–255
    bool     online;
    uint32_t last_seen_ms;
    char     manufacturer[32];                 ///< From Basic cluster 0x0000
    char     model[32];
} zb_device_t;
```

### 2.2 Attribute record

One entry per cluster/attribute pair, per device. Written by the `zigbee_bridge` UART client (decoding H2 sensor reports).

```c
/**
 * @brief Raw Zigbee attribute value.
 *
 * Stored in the sensor state store indexed by (ieee_addr, cluster_id, attr_id).
 * All numeric values normalized to float after ZCL unit conversion.
 * Written exclusively by the zigbee_bridge UART client task (RT-04),
 * which decodes MSG_SENSOR_REPORT frames from the H2 coprocessor.
 */
typedef struct {
    uint16_t cluster_id;      ///< ZCL cluster (e.g. 0x0402 = temp measurement)
    uint16_t attribute_id;    ///< ZCL attribute within cluster
    float    value_float;     ///< Normalized engineering value (°C, %, ppm)
    bool     value_bool;      ///< For binary attributes (dry contact)
    uint8_t  data_type;       ///< ZCL data type tag (raw, for diagnostics)
    uint32_t last_updated_ms;
    bool     valid;           ///< False until first valid report received
} zb_attribute_t;
```

### 2.3 Cluster map — the Layer 1→2 bridge

Single source of truth for all cluster-to-semantic mappings. Add a new sensor type
by adding one row to this table. Nothing else changes.

```c
/**
 * @brief Semantic category of a data point.
 *
 * Used throughout the model to drive binding, aggregation, and BACnet mapping.
 * Aligned with Sowel's DataCategory concept.
 */
typedef enum {
    DATA_CAT_TEMPERATURE,   ///< °C
    DATA_CAT_HUMIDITY,      ///< % RH
    DATA_CAT_CO2,           ///< ppm
    DATA_CAT_OCCUPANCY,     ///< boolean (future)
    DATA_CAT_DRY_CONTACT,   ///< boolean
    DATA_CAT_BATTERY,       ///< % (internal use, not northbound)
    DATA_CAT_UNKNOWN,
} data_category_t;

/**
 * @brief Cluster map entry — ZCL attribute to semantic mapping.
 *
 * Maps a (cluster_id, attribute_id) pair to a semantic category,
 * BACnet object name, unit conversion factor, and plausibility bounds.
 */
typedef struct {
    uint16_t         cluster_id;
    uint16_t         attribute_id;
    data_category_t  category;
    const char      *bacnet_object_name; ///< NULL if not northbound-exposed
    float            scale_factor;       ///< ZCL raw → engineering units
    float            min_plausible;      ///< Values outside range are dropped
    float            max_plausible;
} cluster_map_entry_t;

// The table
static const cluster_map_entry_t CLUSTER_MAP[] = {
    // ZCL 0x0402: Temperature Measurement — attribute 0x0000 MeasuredValue
    // ZCL unit: 1/100 °C (int16). Scale: × 0.01 → °C
    { 0x0402, 0x0000, DATA_CAT_TEMPERATURE, "Zone-Temperature",  0.01f, -40.0f,  80.0f  },

    // ZCL 0x0405: Relative Humidity — attribute 0x0000 MeasuredValue
    // ZCL unit: 1/100 % (uint16). Scale: × 0.01 → %
    { 0x0405, 0x0000, DATA_CAT_HUMIDITY,    "Zone-Humidity",     0.01f,   0.0f, 100.0f  },

    // ZCL 0x040D: Carbon Dioxide (CO2) Concentration — attribute 0x0000
    // ZCL unit: ppm (float32 or uint16 depending on device)
    { 0x040D, 0x0000, DATA_CAT_CO2,         "Zone-CO2",          1.0f,    0.0f, 5000.0f },

    // ZCL 0x000F: Binary Input (Basic) — attribute 0x0055 PresentValue
    { 0x000F, 0x0055, DATA_CAT_DRY_CONTACT, "Zone-DryContact",   1.0f,    0.0f,    1.0f },

    // ZCL 0x0001: Power Configuration — attribute 0x0021 BatteryPercentage
    // ZCL unit: 2× percent (uint8). Scale: × 0.5 → %
    { 0x0001, 0x0021, DATA_CAT_BATTERY,     NULL,                0.5f,    0.0f,  100.0f },
};
#define CLUSTER_MAP_SIZE (sizeof(CLUSTER_MAP) / sizeof(CLUSTER_MAP[0]))
```

---

## 3. Layer 2 — Functional: Equipment model

### 3.1 Data binding

Links an equipment capability (semantic role) to a physical attribute source.
One equipment may have multiple bindings of the same category (e.g. two temperature
sensors contributing to the same equipment average).

```c
#define MAX_BINDINGS_PER_EQUIPMENT  8

/**
 * @brief Binding between an equipment capability and a physical Zigbee attribute.
 *
 * Analogous to Sowel's DataBinding. Resolved at runtime by looking up
 * (device_ieee, cluster_id, attribute_id) in the Zigbee attribute table.
 */
typedef struct {
    data_category_t  category;          ///< Semantic role of this binding
    char             device_ieee[24];   ///< Source device IEEE address
    uint16_t         cluster_id;
    uint16_t         attribute_id;
    char             alias[32];         ///< e.g. "primary-temp", "backup-co2"
    bool             historize;         ///< Log to NVS ring buffer (Tier 3)
} data_binding_t;
```

### 3.2 Equipment capability flags

```c
/**
 * @brief Equipment capability bitmask.
 *
 * Set bits indicate which data categories the equipment exposes.
 * Drives UI rendering, BACnet object instantiation, and aggregation.
 */
typedef enum {
    EQUIP_CAP_TEMPERATURE   = (1 << 0),
    EQUIP_CAP_HUMIDITY      = (1 << 1),
    EQUIP_CAP_CO2           = (1 << 2),
    EQUIP_CAP_DRY_CONTACT   = (1 << 3),
    EQUIP_CAP_HEAT          = (1 << 4),
    EQUIP_CAP_COOL          = (1 << 5),
    EQUIP_CAP_FAN           = (1 << 6),
} equipment_capability_t;
```

### 3.3 Equipment record

```c
#define MAX_EQUIPMENT   4

/**
 * @brief Functional equipment unit.
 *
 * The user-facing and BMS-facing abstraction over physical devices.
 * An equipment lives in a space and binds to one or more Zigbee devices.
 * Analogous to Sowel's Equipment entity.
 */
typedef struct {
    char                 id[16];         ///< Short UUID
    char                 name[48];       ///< "Zone A Sensor Cluster"
    uint32_t             capabilities;   ///< Bitmask of equipment_capability_t
    data_binding_t       bindings[MAX_BINDINGS_PER_EQUIPMENT];
    uint8_t              binding_count;
    char                 space_id[16];   ///< FK → space_t.id (Layer 3)
    bool                 enabled;
} equipment_t;
```

---

## 4. Layer 3 — Topology: Space model

### 4.1 Space types

Commercial building vocabulary. Root is building; leaf is individual space.

```c
/**
 * @brief Spatial hierarchy type.
 *
 * Maps to BMS concepts: a Zone in BACnet terms corresponds to
 * SPACE_TYPE_ZONE or SPACE_TYPE_SPACE here.
 */
typedef enum {
    SPACE_TYPE_BUILDING,  ///< Whole building (root)
    SPACE_TYPE_FLOOR,     ///< Floor level
    SPACE_TYPE_ZONE,      ///< HVAC zone — maps to a BACnet Zone object
    SPACE_TYPE_SPACE,     ///< Individual room or open-plan area
} space_type_t;
```

### 4.2 Aggregated data

Auto-computed by `sensor_state_aggregate_space()` after any equipment update.
Never written directly — only read.

```c
/**
 * @brief Space-level aggregated sensor data.
 *
 * Computed automatically from child equipment bindings.
 * Aggregation is recursive: a parent space aggregates children.
 * AVG for continuous values (temp, humidity, CO2), ANY for boolean.
 */
typedef struct {
    float    avg_temperature;   ///< °C — AVG of temperature-category bindings
    float    avg_humidity;      ///< % — AVG of humidity-category bindings
    float    avg_co2;           ///< ppm — AVG of co2-category bindings
    bool     any_dry_contact;   ///< OR of all dry-contact bindings
    uint8_t  equipment_count;   ///< Total equipment in this space
    uint8_t  online_count;      ///< Equipment with at least one online device
    uint32_t last_updated_ms;
} space_aggregated_data_t;
```

### 4.3 Space record

```c
#define MAX_SPACES              4
#define MAX_EQUIPMENT_PER_SPACE 4

/**
 * @brief Spatial unit in the building topology.
 *
 * Analogous to Sowel's Zone. Holds aggregated sensor data and
 * maps directly to BACnet northbound via bacnet_instance.
 */
typedef struct {
    char                    id[16];
    char                    name[48];              ///< "Floor 3 Zone A"
    space_type_t            type;
    char                    parent_id[16];         ///< Empty string = root
    char                    equipment_ids[MAX_EQUIPMENT_PER_SPACE][16];
    uint8_t                 equipment_count;
    space_aggregated_data_t aggregated;
    uint32_t                bacnet_instance;       ///< BACnet object instance
} space_t;
```

---

## 5. Layer 4 — Orchestration: Modes and control recipes

### 5.1 Mode enums

```c
/**
 * @brief HVAC operating mode.
 * Exposed northbound as BACnet Multi-State Value instance 202.
 */
typedef enum {
    HVAC_MODE_OFF       = 0,
    HVAC_MODE_HEAT      = 1,
    HVAC_MODE_COOL      = 2,
    HVAC_MODE_AUTO      = 3,
    HVAC_MODE_FAN_ONLY  = 4,
    HVAC_MODE_DRY       = 5,
} hvac_mode_t;

/**
 * @brief Occupancy mode — standard BMS/BACnet concept.
 * Exposed northbound as BACnet Multi-State Value instance 203.
 */
typedef enum {
    OCC_MODE_OCCUPIED   = 0,  ///< Full comfort setpoints
    OCC_MODE_UNOCCUPIED = 1,  ///< Reduced setpoints
    OCC_MODE_STANDBY    = 2,  ///< Minimal conditioning
    OCC_MODE_SETBACK    = 3,  ///< Energy-saving offset from comfort
} occupancy_mode_t;
```

### 5.2 Control recipe

Applied per space. Analogous to Sowel's RecipeInstance.
Writable by BACnet task (from BMS) and LVGL UI. Read by control loop.

```c
/**
 * @brief Active control recipe for a space.
 *
 * Contains all parameters needed by the control loop to make
 * relay decisions. Written via sensor_state_set_recipe() only (RT-04).
 * Read by control_loop_tick() on every 1 s cycle.
 */
typedef struct {
    char              id[16];
    char              space_id[16];
    hvac_mode_t       hvac_mode;
    occupancy_mode_t  occupancy_mode;
    float             setpoint_heat;       ///< °C — heating setpoint
    float             setpoint_cool;       ///< °C — cooling setpoint
    float             setpoint_humidity;   ///< % — future use
    float             deadband;            ///< °C — hysteresis around setpoints
    bool              co2_override;        ///< Force ventilation if CO2 > threshold
    float             co2_threshold;       ///< ppm
    bool              dry_contact_lockout; ///< Interlock: disable output if contact open
} control_recipe_t;
```

---

## 6. Layer 5 — BACnet/SC object map

### 6.1 Instance allocation strategy

```
Instances   0–99:   Space-level aggregated values (per space)
Instances 100–199:  Equipment-level raw values (per equipment)
Instances 200–299:  Control — setpoints, modes, commands
Instances 300–399:  Diagnostics — RT health, Zigbee LQI, battery, NVS wear
```

### 6.2 Object map table

```c
/**
 * @brief BACnet object map entry.
 *
 * Defines the full northbound identity of one BACnet object.
 * The map is the single place where internal model concepts
 * are translated to BACnet protocol surface.
 */
typedef struct {
    uint32_t          instance;
    const char       *object_name;        ///< BACnet object name (unique per device)
    const char       *description;        ///< Human-readable, shown in BMS
    bacnet_obj_type_t type;               ///< AI, BI, AV, BO, MSV...
    data_category_t   source_category;    ///< DATA_CAT_* driving the value, or -1
    char              source_space_id[16];///< Which space drives this object
    bool              cov_enabled;        ///< COV subscriptions allowed
    float             cov_increment;      ///< Min Δ to trigger COV notification
} bacnet_object_map_entry_t;

// Single-zone deployment map
static const bacnet_object_map_entry_t BACNET_OBJECT_MAP[] = {
    // Space aggregated — primary BMS interface
    {0,   "Zone-Temperature",     "Avg temperature Zone A",    OBJ_ANALOG_INPUT,     DATA_CAT_TEMPERATURE, "zone_a", true,  0.1f },
    {1,   "Zone-Humidity",        "Avg humidity Zone A",       OBJ_ANALOG_INPUT,     DATA_CAT_HUMIDITY,    "zone_a", true,  1.0f },
    {2,   "Zone-CO2",             "CO2 concentration Zone A",  OBJ_ANALOG_INPUT,     DATA_CAT_CO2,         "zone_a", true,  25.0f},
    {3,   "Zone-DryContact",      "Dry contact Zone A",        OBJ_BINARY_INPUT,     DATA_CAT_DRY_CONTACT, "zone_a", true,  0.0f },

    // Control — writable by BMS
    {200, "Zone-Setpoint-Heat",   "Heating setpoint",          OBJ_ANALOG_VALUE,     -1, "zone_a", true,  0.5f },
    {201, "Zone-Setpoint-Cool",   "Cooling setpoint",          OBJ_ANALOG_VALUE,     -1, "zone_a", true,  0.5f },
    {202, "Zone-HVAC-Mode",       "HVAC mode",                 OBJ_MULTI_STATE_VALUE,-1, "zone_a", true,  0.0f },
    {203, "Zone-Occ-Mode",        "Occupancy mode",            OBJ_MULTI_STATE_VALUE,-1, "zone_a", true,  0.0f },

    // Relay outputs — readable by BMS (actual state)
    {204, "Relay-Heat",           "Heat relay output",         OBJ_BINARY_OUTPUT,    -1, "zone_a", false, 0.0f },
    {205, "Relay-Cool",           "Cool relay output",         OBJ_BINARY_OUTPUT,    -1, "zone_a", false, 0.0f },
    {206, "Relay-Fan",            "Fan relay output",          OBJ_BINARY_OUTPUT,    -1, "zone_a", false, 0.0f },

    // Diagnostics — BMS observability of RT health
    {300, "Diag-DeadlineMisses",  "Control loop deadline misses", OBJ_ANALOG_INPUT,  -1, NULL,     false, 0.0f },
    {301, "Diag-ZigbeeLQI",       "Zigbee average LQI",           OBJ_ANALOG_INPUT,  -1, NULL,     false, 1.0f },
    {302, "Diag-BatteryMin",      "Lowest sensor battery %",      OBJ_ANALOG_INPUT,  -1, NULL,     false, 5.0f },
    {303, "Diag-NVSWrites",       "NVS commit count (flash wear)", OBJ_ANALOG_INPUT, -1, NULL,     false, 1.0f },
};
#define BACNET_OBJECT_MAP_SIZE (sizeof(BACNET_OBJECT_MAP) / sizeof(BACNET_OBJECT_MAP[0]))
```

---

## 7. Central sensor state store

The runtime spine — all five layers live here. Mutex-protected. Access via API only (RT-04).

```c
/**
 * @brief Central thermostat runtime state.
 *
 * Single shared struct holding all five model layers.
 * Protected by a FreeRTOS mutex. Never accessed directly
 * across task boundaries — use sensor_state_*() API only (RT-04).
 *
 * Allocated statically in sensor_state_init(). Never freed.
 */
typedef struct {
    // Layer 1 — physical Zigbee state (written by zigbee_bridge UART client)
    zb_device_t      devices[MAX_ZB_DEVICES];
    uint8_t          device_count;
    zb_attribute_t   attributes[MAX_ZB_DEVICES][MAX_ZB_CLUSTERS];

    // Layer 2 — functional equipment (written by aggregation pass)
    equipment_t      equipment[MAX_EQUIPMENT];
    uint8_t          equipment_count;

    // Layer 3 — spatial topology (written by aggregation pass)
    space_t          spaces[MAX_SPACES];
    uint8_t          space_count;

    // Layer 4 — active control recipe (written by BACnet task or LVGL UI)
    control_recipe_t active_recipe;

    // Layer 5 — BACnet mirror state
    uint32_t         bacnet_cov_sequence; ///< Incremented on each COV notification

    // Diagnostics (written by rt_monitor task; NVS fields set once at boot)
    uint32_t         rt_deadline_miss_count;
    uint8_t          zigbee_avg_lqi;
    uint8_t          battery_min_pct;
    uint32_t         nvs_write_count;   ///< NVS commits — BACnet AI 303 (flash wear)
    bool             nvs_recovered;     ///< NVS corruption recovery ran at boot

    // Metadata
    uint32_t         last_update_ms;
    SemaphoreHandle_t mutex;             ///< Max hold: RT-05 budget = 1 ms
} thermostat_state_t;
```

### 7.1 Public API — the only legal cross-task interface

```c
// Lifecycle
esp_err_t sensor_state_init(void);

// Layer 1 writes (zigbee_bridge UART client → store)
esp_err_t sensor_state_update_attribute(const char *ieee, uint16_t cluster,
                                         uint16_t attr, float value);
esp_err_t sensor_state_register_device(const zb_device_t *device);
esp_err_t sensor_state_set_device_online(const char *ieee, bool online);

// Layer 3 reads (control loop, BACnet task, LVGL)
esp_err_t sensor_state_get_space(const char *space_id, space_t *out);
esp_err_t sensor_state_get_bacnet_value(uint32_t instance, float *out);

// Layer 4 writes (BACnet task or LVGL → control loop)
esp_err_t sensor_state_set_recipe(const control_recipe_t *recipe);
esp_err_t sensor_state_get_recipe(control_recipe_t *out);

// Diagnostics
uint32_t  sensor_state_get_deadline_misses(void);
void      sensor_state_increment_deadline_miss(void);
```

---

## 8. Data management tiers

### Tier 1 — Runtime (RAM)
`thermostat_state_t` in RAM. Lost on reboot — acceptable because sensors
re-report within one cycle (~30 s). Working memory only.

### Tier 2 — Persistent config (NVS)
Written only on commissioning or user change. Never on sensor update.

| NVS namespace | Contents |
|---|---|
| `zb_devices` | Zigbee device table (IEEE, clusters, friendly names) |
| `topology` | Space/equipment configuration |
| `recipe` | Active control recipe (setpoints, modes) |
| `bacnet_cfg` | BACnet/SC network config, device instance |
| `wifi_cfg` | Wi-Fi credentials |
| `certs` | BACnet/SC X.509 certificates |
| `ota_meta` | OTA version history, anti-rollback counter |

### Tier 3 — Historical ring buffer (NVS)
5-minute samples of temperature, humidity, CO₂ per space.
Depth: ~7 days = 2016 samples × 3 values × 4 bytes = ~24 KB.
Dedicated NVS partition (`history`, 32 KB). Circular overwrite.
Used by LVGL trending graph and BACnet Trend Log objects.
Future: push to InfluxDB or Sowel on BMS request.

**Key rule: no SQLite, no SPIFFS on the thermostat.**
NVS only. The thermostat is a real-time edge device, not a database.

---

## 9. Data flow — end to end

```
MSG_SENSOR_REPORT frame received over UART bridge (from H2 coordinator)
    │
    ▼ zigbee_bridge UART client task
zb_attribute_t updated in state store
    │
    ▼ cluster_map_lookup()
data_category_t resolved + value converted + plausibility checked
    │
    ▼ sensor_state_aggregate_space()
space_t.aggregated recomputed (AVG/ANY over all bindings)
    │
    ├──► bacnet_server task notified (event group bit)
    │         │
    │         ▼ BACnet object present value updated
    │           COV notification sent if Δ > cov_increment
    │
    ├──► control_loop_tick() reads space.aggregated on next 1 s cycle
    │         │
    │         ▼ relay decision (heat/cool/fan)
    │           hal_gpio_set(HAL_GPIO_RELAY_HEAT, true/false)
    │
    └──► lvgl_ui refreshes temperature / humidity / CO2 display
```

---

## 11. BLE commissioning interface (all SKUs)

All three SKUs include BLE 5 commissioning at zero additional hardware cost
(ESP32-C6 has BLE on-chip). The commissioning layer writes directly to the
sensor state store via the Layer 4 control recipe and Layer 2 equipment bindings.

```c
// commissioning.h — same API on all SKUs
esp_err_t commissioning_init(void);
esp_err_t commissioning_start_advertising(void);
esp_err_t commissioning_set_wifi_credentials(const char *ssid, const char *pass);
esp_err_t commissioning_set_bacnet_config(const bacnet_config_t *cfg);
esp_err_t commissioning_set_recipe(const control_recipe_t *recipe);
esp_err_t commissioning_bind_device(const char *ieee, const char *space_id);
```

On TH-HEADLESS this is the **primary** configuration interface.
On TH-DISPLAY this supplements the touchscreen menu.
On TH-SEGMENT this supplements the limited button interface.

## 12. Sowel integration path (future)

The five-layer model is designed to be compatible with Sowel's plugin architecture.
When a Sowel integration is desired:

- Layer 1 (`zb_device_t`) maps to Sowel `Device`
- Layer 2 (`equipment_t`) maps to Sowel `Equipment`
- Layer 3 (`space_t`) maps to Sowel `Zone`
- Layer 4 (`control_recipe_t`) maps to Sowel `RecipeInstance`
- Layer 5 (BACnet map) becomes a Sowel integration plugin adapter

The firmware's internal model requires no changes — only a new Layer 5 transport
(Sowel MQTT/REST instead of BACnet/SC) is needed.
