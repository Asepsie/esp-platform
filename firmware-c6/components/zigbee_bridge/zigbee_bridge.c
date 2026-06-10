/**
 * @file zigbee_bridge.c
 * @brief C6 UART bridge — core decode + dispatch (host- and target-compiled).
 *
 * Frame processing, H2 liveness, and lifecycle. Intentionally free of FreeRTOS,
 * esp_log, and driver/UART code so it compiles for host unit tests: the UART RX
 * task and command TX live in zigbee_bridge_io.c (target) / zigbee_bridge_mock.c
 * (host). Time comes from hal_timer (mockable). Decoded data is written straight
 * into sensor_state.
 */
#include "zigbee_bridge.h"
#include "zigbee_bridge_internal.h"
#include "uart_bridge_protocol.h"
#include "sensor_state.h"
#include "cluster_map.h"
#include "hal_timer.h"
#include "thermostat_config.h"   // H2_HEARTBEAT_TIMEOUT_MS

#include <string.h>
#include <stdio.h>

static bridge_report_cb_t s_report_cb;
static bridge_join_cb_t   s_join_cb;
static bool               s_h2_seen;
static uint32_t           s_last_heartbeat_ms;

// Format an 8-byte Zigbee IEEE address as the "0x...." string used as the
// device key in sensor_state. The SAME conversion is used for joins and
// reports, so the strings always match.
static void ieee_to_str(const uint8_t ieee[8], char out[IEEE_ADDR_STR_LEN])
{
    snprintf(out, IEEE_ADDR_STR_LEN,
             "0x%02x%02x%02x%02x%02x%02x%02x%02x",
             ieee[0], ieee[1], ieee[2], ieee[3],
             ieee[4], ieee[5], ieee[6], ieee[7]);
}

// SENSOR_REPORT → cluster_map (pick float vs bool value) → store.
static void handle_sensor_report(const bridge_sensor_report_t *r)
{
    const cluster_map_entry_t *cm =
        cluster_map_lookup(r->cluster_id, r->attribute_id);
    if (cm == NULL) {
        return; // unmapped (cluster, attribute) — ignore
    }

    // Boolean categories carry their value in value_bool; others in value_float.
    float value;
    if (cm->category == DATA_CAT_DRY_CONTACT || cm->category == DATA_CAT_OCCUPANCY) {
        value = r->value_bool ? 1.0f : 0.0f;
    } else {
        value = r->value_float;
    }

    char ieee[IEEE_ADDR_STR_LEN];
    ieee_to_str(r->ieee_addr, ieee);
    // Updates the store and triggers space re-aggregation (RT-04 API).
    (void)sensor_state_update_attribute(ieee, r->cluster_id, r->attribute_id, value);
}

// DEVICE_JOIN → register the device in the store.
static void handle_device_join(const bridge_device_join_t *d)
{
    zb_device_t dev;
    memset(&dev, 0, sizeof(dev));
    ieee_to_str(d->ieee_addr, dev.ieee_addr);
    dev.short_addr = d->short_addr;
    dev.cluster_count = d->cluster_count;
    for (int i = 0; i < MAX_ZB_CLUSTERS && i < 8; i++) {
        dev.supported_clusters[i] = d->supported_clusters[i];
    }
    strncpy(dev.manufacturer, d->manufacturer, sizeof(dev.manufacturer) - 1);
    strncpy(dev.model, d->model, sizeof(dev.model) - 1);
    dev.online = true;
    dev.last_seen_ms = hal_timer_get_ms();
    (void)sensor_state_register_device(&dev);
}

void zigbee_bridge_process_frame(const uint8_t *frame, size_t len)
{
    bridge_msg_type_t type;
    uint8_t  payload[BRIDGE_MAX_PAYLOAD];
    uint16_t plen = 0;

    bridge_decode_status_t st =
        bridge_frame_decode(frame, len, &type, payload, sizeof(payload), &plen);
    if (st != BRIDGE_DECODE_OK) {
        return; // bad CRC / truncated / wrong SOF — drop silently
    }

    switch (type) {
    case MSG_SENSOR_REPORT:
        if (plen >= sizeof(bridge_sensor_report_t)) {
            bridge_sensor_report_t r;
            memcpy(&r, payload, sizeof(r));
            handle_sensor_report(&r);
            if (s_report_cb != NULL) {
                s_report_cb(&r);
            }
        }
        break;

    case MSG_DEVICE_JOIN:
        if (plen >= sizeof(bridge_device_join_t)) {
            bridge_device_join_t d;
            memcpy(&d, payload, sizeof(d));
            handle_device_join(&d);
            if (s_join_cb != NULL) {
                s_join_cb(&d);
            }
        }
        break;

    case MSG_HEARTBEAT:
        s_h2_seen = true;
        s_last_heartbeat_ms = hal_timer_get_ms();
        break;

    default:
        // DEVICE_LEAVE / DEVICE_STATUS / NACK / OTA acks: handled in later work.
        break;
    }
}

bool zigbee_bridge_is_h2_online(void)
{
    if (!s_h2_seen) {
        return false;
    }
    uint32_t elapsed = hal_timer_get_ms() - s_last_heartbeat_ms;
    return elapsed < H2_HEARTBEAT_TIMEOUT_MS;
}

void zigbee_bridge_set_callbacks(bridge_report_cb_t report_cb,
                                 bridge_join_cb_t join_cb)
{
    s_report_cb = report_cb;
    s_join_cb = join_cb;
}

esp_err_t zigbee_bridge_init(void)
{
    s_report_cb = NULL;
    s_join_cb = NULL;
    s_h2_seen = false;
    s_last_heartbeat_ms = 0;
    return zigbee_bridge_io_start();
}
