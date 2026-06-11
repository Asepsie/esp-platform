// =============================================================================
// zigbee_cluster_handler.c — ZCL attribute -> bridge_sensor_report_t.
//
// See zigbee_cluster_handler.h. Pure C; host-unit-tested by
// tests/host/tests/test_cluster_handler.c. No esp-zigbee-sdk dependency.
// =============================================================================
#include "zigbee_cluster_handler.h"

#include <string.h>

// Warn-level log for a rejected (implausible) reading. On target this is
// ESP_LOGW; in host unit tests it is a no-op (logging is a target concern, and
// the tests assert on the return code, not on log output).
#if defined(ESP_PLATFORM)
#include "esp_log.h"
#define CH_LOGW(fmt, ...) ESP_LOGW("zb_cluster", fmt, ##__VA_ARGS__)
#else
#define CH_LOGW(fmt, ...) ((void)0)
#endif

// --- ZCL data type tags (the subset we decode) -------------------------------
// From the ZCL spec's data-type table. Anything not listed here is treated as
// undecodable rather than guessed at.
#define ZCL_TYPE_BOOL     0x10  // boolean, 1 byte (0x00/0x01; 0xFF = invalid)
#define ZCL_TYPE_U8       0x20  // uint8
#define ZCL_TYPE_U16      0x21  // uint16
#define ZCL_TYPE_U24      0x22  // uint24
#define ZCL_TYPE_U32      0x23  // uint32
#define ZCL_TYPE_S8       0x28  // int8
#define ZCL_TYPE_S16      0x29  // int16
#define ZCL_TYPE_S32      0x2b  // int32
#define ZCL_TYPE_SINGLE   0x39  // IEEE-754 single precision (float32)

// How a mapped attribute's value is represented in the bridge report.
typedef enum {
    VAL_ANALOG,   // numeric -> report.value_float (engineering units)
    VAL_BINARY,   // boolean -> report.value_bool
} val_kind_t;

// One mapped (cluster, attribute). Mirrors firmware-c6 cluster_map.c.
// scale: ZCL raw -> engineering units; [min,max] plausibility in eng. units.
typedef struct {
    uint16_t   cluster_id;
    uint16_t   attribute_id;
    val_kind_t kind;
    float      scale;
    float      min_plausible;
    float      max_plausible;
    bool       is_battery;   // also populate report.battery_pct from the value
} attr_map_t;

static const attr_map_t ATTR_MAP[] = {
    // ZCL 0x0402 Temperature Measurement / 0x0000 MeasuredValue (s16, 1/100 °C).
    { 0x0402, 0x0000, VAL_ANALOG, 0.01f, -40.0f,   80.0f, false },
    // ZCL 0x0405 Relative Humidity / 0x0000 MeasuredValue (u16, 1/100 %).
    { 0x0405, 0x0000, VAL_ANALOG, 0.01f,   0.0f,  100.0f, false },
    // ZCL 0x040D CO2 Measurement / 0x0000 MeasuredValue (uint16 or single, ppm).
    // Mirrors the C6 map (scale 1.0, 0–5000 ppm): the producer emits ppm
    // directly. NOTE: standard ZCL types this attribute as a `single` fraction
    // (mol/mol, e.g. 0.0004 = 400 ppm). Confirm against the real sensor and, if
    // it reports the ZCL fraction, change scale to 1.0e6f — the one line to touch.
    { 0x040D, 0x0000, VAL_ANALOG, 1.0f,    0.0f, 5000.0f, false },
    // ZCL 0x000F Binary Input (Basic) / 0x0055 PresentValue (bool, dry contact).
    { 0x000F, 0x0055, VAL_BINARY, 1.0f,    0.0f,    1.0f, false },
    // ZCL 0x0001 Power Configuration / 0x0021 BatteryPercentageRemaining
    // (u8, 2× %). Also copied into report.battery_pct.
    { 0x0001, 0x0021, VAL_ANALOG, 0.5f,    0.0f,  100.0f, true  },
};
#define ATTR_MAP_SIZE (sizeof(ATTR_MAP) / sizeof(ATTR_MAP[0]))

static const attr_map_t *map_lookup(uint16_t cluster_id, uint16_t attribute_id)
{
    for (size_t i = 0; i < ATTR_MAP_SIZE; i++) {
        if (ATTR_MAP[i].cluster_id == cluster_id &&
            ATTR_MAP[i].attribute_id == attribute_id) {
            return &ATTR_MAP[i];
        }
    }
    return NULL;
}

// Byte width of a ZCL data type, or 0 if this handler does not decode it.
static uint8_t type_width(uint8_t data_type)
{
    switch (data_type) {
        case ZCL_TYPE_BOOL:
        case ZCL_TYPE_U8:
        case ZCL_TYPE_S8:     return 1;
        case ZCL_TYPE_U16:
        case ZCL_TYPE_S16:    return 2;
        case ZCL_TYPE_U24:    return 3;
        case ZCL_TYPE_U32:
        case ZCL_TYPE_S32:
        case ZCL_TYPE_SINGLE: return 4;
        default:              return 0;
    }
}

// Decode the leading bytes of `raw` (little-endian) per `data_type` into a
// double. Caller guarantees raw_len >= type_width(data_type) > 0. Signed types
// are sign-extended; SINGLE is reinterpreted as IEEE-754 float32.
static double decode_raw(uint8_t data_type, const uint8_t *raw)
{
    switch (data_type) {
        case ZCL_TYPE_BOOL:
        case ZCL_TYPE_U8:
            return (double)raw[0];
        case ZCL_TYPE_S8:
            return (double)(int8_t)raw[0];
        case ZCL_TYPE_U16:
            return (double)(uint16_t)((uint16_t)raw[0] | ((uint16_t)raw[1] << 8));
        case ZCL_TYPE_S16:
            return (double)(int16_t)((uint16_t)raw[0] | ((uint16_t)raw[1] << 8));
        case ZCL_TYPE_U24:
            return (double)((uint32_t)raw[0] | ((uint32_t)raw[1] << 8) |
                            ((uint32_t)raw[2] << 16));
        case ZCL_TYPE_U32:
            return (double)((uint32_t)raw[0] | ((uint32_t)raw[1] << 8) |
                            ((uint32_t)raw[2] << 16) | ((uint32_t)raw[3] << 24));
        case ZCL_TYPE_S32:
            return (double)(int32_t)((uint32_t)raw[0] | ((uint32_t)raw[1] << 8) |
                            ((uint32_t)raw[2] << 16) | ((uint32_t)raw[3] << 24));
        case ZCL_TYPE_SINGLE: {
            uint32_t bits = (uint32_t)raw[0] | ((uint32_t)raw[1] << 8) |
                            ((uint32_t)raw[2] << 16) | ((uint32_t)raw[3] << 24);
            float f;
            memcpy(&f, &bits, sizeof(f));
            return (double)f;
        }
        default:
            return 0.0;  // unreachable: width-checked by the caller
    }
}

bool zigbee_cluster_handler_is_supported(uint16_t cluster_id, uint16_t attr_id)
{
    return map_lookup(cluster_id, attr_id) != NULL;
}

esp_err_t zigbee_cluster_handler_process(const uint8_t *src_ieee,
                                         uint16_t cluster_id,
                                         uint16_t attr_id,
                                         uint8_t data_type,
                                         const uint8_t *raw_data,
                                         size_t raw_len,
                                         uint8_t lqi,
                                         bridge_sensor_report_t *report)
{
    if (src_ieee == NULL || raw_data == NULL || report == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const attr_map_t *m = map_lookup(cluster_id, attr_id);
    if (m == NULL) {
        return ESP_ERR_NOT_FOUND;           // attribute we don't forward
    }

    const uint8_t width = type_width(data_type);
    if (width == 0 || raw_len < width) {
        return ESP_ERR_INVALID_ARG;         // undecodable type or truncated payload
    }

    const double raw_value = decode_raw(data_type, raw_data);

    // Analog readings are range-checked BEFORE the report is written, so a
    // rejected (implausible) value never partially fills the report.
    bool value_bool = false;
    float value_float;
    uint8_t battery_pct = 0xFF;             // unknown unless this is a battery report

    if (m->kind == VAL_BINARY) {
        value_bool  = (raw_value != 0.0);   // any nonzero present-value is "active"
        value_float = value_bool ? 1.0f : 0.0f;
    } else {
        const double eng = raw_value * (double)m->scale;
        if (eng < (double)m->min_plausible || eng > (double)m->max_plausible) {
            CH_LOGW("drop cluster 0x%04X attr 0x%04X: %.3f out of [%.1f, %.1f]",
                    cluster_id, attr_id, eng,
                    (double)m->min_plausible, (double)m->max_plausible);
            return ESP_ERR_INVALID_ARG;
        }
        value_float = (float)eng;
        if (m->is_battery) {
            // Surface battery in its dedicated field too, clamped to 0–100. eng
            // is non-negative here, so +0.5 truncation rounds to nearest (no libm).
            const double pct = eng > 100.0 ? 100.0 : eng;
            battery_pct = (uint8_t)(pct + 0.5);
        }
    }

    memcpy(report->ieee_addr, src_ieee, sizeof(report->ieee_addr));
    report->cluster_id   = cluster_id;
    report->attribute_id = attr_id;
    report->data_type    = data_type;
    report->value_float  = value_float;
    report->value_bool   = value_bool;
    report->lqi          = lqi;
    report->battery_pct  = battery_pct;
    return ESP_OK;
}
