/**
 * @file zigbee_bridge_internal.h
 * @brief Private interface shared between the bridge core, the target I/O
 *        layer (zigbee_bridge_io.c), and the host mock (zigbee_bridge_mock.c).
 *
 * Not part of the public API.
 */
#ifndef ZIGBEE_BRIDGE_INTERNAL_H
#define ZIGBEE_BRIDGE_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include "platform_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Decode one complete frame buffer and dispatch it (core logic).
 *
 * Decodes with the shared framing; on success routes by message type into the
 * sensor_state store (and observer callbacks) and updates H2 liveness. Invalid
 * frames (bad CRC, truncated, wrong SOF, unmapped attribute) are dropped.
 * This is the single entry point exercised by the RX task (target) and by the
 * mock's frame-injection helpers (host).
 *
 * @param frame Pointer to a complete frame.
 * @param len   Number of bytes available in @p frame.
 */
void zigbee_bridge_process_frame(const uint8_t *frame, size_t len);

/**
 * @brief Bring up the UART I/O layer and start the RX task.
 *
 * Defined in zigbee_bridge_io.c on target (hal_uart + RX task); the host mock
 * provides a no-op. Called by zigbee_bridge_init().
 *
 * @return @c ESP_OK on success.
 */
esp_err_t zigbee_bridge_io_start(void);

#ifdef __cplusplus
}
#endif

#endif /* ZIGBEE_BRIDGE_INTERNAL_H */
