/**
 * @file zigbee_bridge_mock.h
 * @brief Host test seam for the zigbee_bridge — inject frames as if from H2.
 *
 * The mock (@c zigbee_bridge_mock.c) replaces the target I/O layer: instead of a
 * real UART RX task, tests push bytes straight into the core
 * @c zigbee_bridge_process_frame(). This exercises the real decode → cluster
 * map → sensor_state path end-to-end on host, no hardware.
 */
#ifndef ZIGBEE_BRIDGE_MOCK_H
#define ZIGBEE_BRIDGE_MOCK_H

#include <stddef.h>
#include <stdint.h>
#include "uart_bridge_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Feed a raw frame into the bridge as if received over UART from the H2.
 * @param raw Complete frame bytes (SOF..CRC).
 * @param len Number of bytes.
 */
void zigbee_bridge_mock_inject_frame(const uint8_t *raw, size_t len);

/**
 * @brief Encode @p r as a SENSOR_REPORT frame and inject it (convenience).
 * @param r Sensor report payload.
 */
void zigbee_bridge_mock_inject_sensor_report(const bridge_sensor_report_t *r);

#ifdef __cplusplus
}
#endif

#endif /* ZIGBEE_BRIDGE_MOCK_H */
