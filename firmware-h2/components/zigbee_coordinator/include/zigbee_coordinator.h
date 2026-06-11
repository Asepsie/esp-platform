// =============================================================================
// zigbee_coordinator.h — H2 Zigbee 3.0 coordinator (target-only).
//
// Forms a centralized Zigbee network on the configured channel, receives ZCL
// attribute reports from joined devices, runs them through the (host-tested)
// zigbee_cluster_handler, and hands the resulting bridge_sensor_report_t /
// bridge_device_join_t to registered callbacks (which forward them to the C6
// over the UART bridge).
//
// This module calls esp-zigbee-sdk APIs and CANNOT be host-tested — it is
// validated by target build + real hardware. The pure conversion logic it
// depends on lives in zigbee_cluster_handler.{c,h} (host-tested).
// =============================================================================
#ifndef ZIGBEE_COORDINATOR_H
#define ZIGBEE_COORDINATOR_H

#include <stdint.h>
#include "esp_err.h"
#include "uart_bridge_protocol.h" // bridge_sensor_report_t, bridge_device_join_t

#ifdef __cplusplus
extern "C" {
#endif

/** Called when a joined device sends an attribute report we understand. */
typedef void (*zb_report_cb_t)(const bridge_sensor_report_t *r);

/** Called when a new device joins the network. */
typedef void (*zb_join_cb_t)(const bridge_device_join_t *d);

/**
 * @brief Initialize the Zigbee coordinator.
 *
 * Configures the native 802.15.4 radio, initializes the stack as a coordinator,
 * registers the coordinator endpoint and the attribute-report handler, and sets
 * the primary channel. The network is formed asynchronously after
 * zigbee_coordinator_start() and is CLOSED by default — no device can join
 * until zigbee_coordinator_permit_join() is called.
 *
 * Register the report/join callbacks (below) before calling this.
 */
esp_err_t zigbee_coordinator_init(void);

/**
 * @brief Start the Zigbee coordinator task.
 *
 * Launches the Zigbee stack main loop as a FreeRTOS task. Must be called after
 * zigbee_coordinator_init().
 */
esp_err_t zigbee_coordinator_start(void);

/**
 * @brief Open or close the network for device joining.
 *
 * Safe to call from any task (acquires the Zigbee stack lock).
 *
 * @param duration_s  Join window in seconds (0 = close, 254 = effectively permanent)
 */
esp_err_t zigbee_coordinator_permit_join(uint8_t duration_s);

/**
 * @brief Request an immediate attribute read from a joined device.
 *
 * Issues a ZCL Read Attributes command; the response arrives asynchronously via
 * the normal attribute-report path (the registered report callback). Safe to
 * call from any task. Backs MSG_POLL_ATTR from the C6.
 *
 * @param ieee        Target device IEEE address (8 bytes)
 * @param cluster_id  ZCL cluster ID to read
 * @param attr_id     ZCL attribute ID to read
 */
esp_err_t zigbee_coordinator_poll_attr(const uint8_t *ieee, uint16_t cluster_id,
                                       uint16_t attr_id);

/** @brief Register the callback invoked for each understood sensor report. */
esp_err_t zigbee_coordinator_register_report_cb(zb_report_cb_t cb);

/** @brief Register the callback invoked when a device joins. */
esp_err_t zigbee_coordinator_register_join_cb(zb_join_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif // ZIGBEE_COORDINATOR_H
