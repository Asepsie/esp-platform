/**
 * @file zigbee_bridge.h
 * @brief C6 side of the UART bridge — receives H2 sensor data into the store.
 *
 * The counterpart to the H2 `uart_bridge`. A UART RX task (RT-01:
 * `zigbee_bridge_rx`, prio 6, 8 KB) reads frames from the H2 over the bridge
 * UART, decodes them with the shared protocol/framing, and feeds them into the
 * sensor_state store: SENSOR_REPORT → `sensor_state_update_attribute()`,
 * DEVICE_JOIN → `sensor_state_register_device()`. HEARTBEAT frames maintain H2
 * liveness; absence for H2_HEARTBEAT_TIMEOUT_MS marks the H2 offline.
 *
 * UART details are hidden behind hal_uart; this is the only consumer of the
 * bridge protocol on the C6.
 *
 * @see docs/architecture/uart-bridge-protocol.md (§8 C6 side).
 * @see docs/architecture/data-model-v2.md (§9 data flow).
 */
#ifndef ZIGBEE_BRIDGE_H
#define ZIGBEE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "platform_compat.h"      /* esp_err_t */
#include "uart_bridge_protocol.h" /* bridge_sensor_report_t, bridge_device_join_t */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Optional observer invoked for each decoded SENSOR_REPORT. */
typedef void (*bridge_report_cb_t)(const bridge_sensor_report_t *report);

/** @brief Optional observer invoked for each decoded DEVICE_JOIN. */
typedef void (*bridge_join_cb_t)(const bridge_device_join_t *device);

/**
 * @brief Initialize the bridge: reset state and start the UART RX task.
 *
 * On target this brings up hal_uart and launches the RX task. The bridge writes
 * decoded reports/joins directly into sensor_state; observer callbacks (below)
 * are optional extras.
 *
 * @return @c ESP_OK on success, or an @c esp_err_t from the UART/task setup.
 */
esp_err_t zigbee_bridge_init(void);

/**
 * @brief Register optional observer callbacks (NULL to clear).
 *
 * Invoked after the store has been updated, for UI/logging hooks. The store is
 * updated regardless of whether callbacks are set.
 */
void zigbee_bridge_set_callbacks(bridge_report_cb_t report_cb,
                                 bridge_join_cb_t join_cb);

/**
 * @brief Ask the H2 to open the Zigbee network for joining.
 * @param duration_s 0 = close, 254 = open indefinitely.
 * @return @c ESP_OK on success, or an @c esp_err_t.
 */
esp_err_t zigbee_bridge_permit_join(uint8_t duration_s);

/**
 * @brief Ask the H2 to immediately poll an attribute from a device.
 * @param ieee    8-byte Zigbee IEEE address (non-NULL).
 * @param cluster ZCL cluster id.
 * @param attr    ZCL attribute id.
 * @return @c ESP_OK on success, or an @c esp_err_t.
 */
esp_err_t zigbee_bridge_poll_attribute(const uint8_t *ieee,
                                       uint16_t cluster, uint16_t attr);

/**
 * @brief Whether the H2 is considered online.
 * @return @c true if a heartbeat was seen within H2_HEARTBEAT_TIMEOUT_MS.
 */
bool zigbee_bridge_is_h2_online(void);

#ifdef __cplusplus
}
#endif

#endif /* ZIGBEE_BRIDGE_H */
