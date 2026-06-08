// =============================================================================
// uart_bridge.h — H2-facing interface to the C6 <-> H2 UART bridge.
//
// This is the H2 *driver* layer: it owns UART1, runs the RX task that decodes
// incoming command frames from the C6, and provides helpers to send reports /
// heartbeats back. The wire format and shared payload types live in
// uart_bridge_protocol.h; the pure encode/decode/CRC lives in
// uart_bridge_framing.c (host-tested).
//
// Typical use (on the H2):
//   uart_bridge_init(on_cmd);                 // start UART + RX task
//   uart_bridge_send_sensor_report(&report);  // forward a Zigbee attribute
//   // on_cmd() fires for each C6->H2 command frame (PERMIT_JOIN, POLL_ATTR, ...)
// =============================================================================
#ifndef UART_BRIDGE_H
#define UART_BRIDGE_H

#include "esp_err.h"
#include "uart_bridge_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

// Called from the RX task for every successfully decoded C6 -> H2 frame.
// `payload` points into the RX task's buffer and is only valid for the duration
// of the call — copy anything you need to keep. Keep this handler short.
typedef void (*bridge_cmd_cb_t)(bridge_msg_type_t type,
                                const uint8_t *payload, uint16_t len);

// Bring up the bridge: configure UART, start the RX task, and start the 5 s
// heartbeat task. `cmd_cb` may be NULL if the caller does not handle commands
// yet. Returns ESP_OK on success.
esp_err_t uart_bridge_init(bridge_cmd_cb_t cmd_cb);

// Send a Zigbee attribute report to the C6 (MSG_SENSOR_REPORT).
esp_err_t uart_bridge_send_sensor_report(const bridge_sensor_report_t *report);

// Send a device-join notification to the C6 (MSG_DEVICE_JOIN).
esp_err_t uart_bridge_send_device_join(const bridge_device_join_t *device);

// Send one heartbeat (MSG_HEARTBEAT) with the next sequence number. The bridge
// also sends these automatically every 5 s; this is exposed for tests/manual use.
esp_err_t uart_bridge_send_heartbeat(void);

// Link health / debug counters.
typedef struct {
    uint32_t rx_frames_ok;       // frames decoded and dispatched
    uint32_t rx_crc_errors;      // frames dropped on CRC mismatch (NACK sent)
    uint32_t rx_framing_errors;  // bad SOF / bad length / timeout mid-frame
    uint32_t tx_frames;          // frames written to the UART
} uart_bridge_stats_t;

// Copy the current counters into `out` (no-op if `out` is NULL).
void uart_bridge_get_stats(uart_bridge_stats_t *out);

#ifdef __cplusplus
}
#endif

#endif // UART_BRIDGE_H
