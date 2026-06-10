/**
 * @file zigbee_bridge_mock.c
 * @brief Host mock of the zigbee_bridge I/O layer.
 *
 * Replaces zigbee_bridge_io.c for host tests: the I/O start is a no-op and the
 * command-direction API is stubbed (no UART on host). Adds frame-injection
 * helpers that drive the real core decode/dispatch path. Plain C, no ESP-IDF.
 */
#include "zigbee_bridge.h"
#include "zigbee_bridge_internal.h"
#include "zigbee_bridge_mock.h"
#include "uart_bridge_protocol.h"

// --- I/O layer replaced for host (no UART, no task) --------------------------

esp_err_t zigbee_bridge_io_start(void)
{
    return ESP_OK; // nothing to bring up on host
}

esp_err_t zigbee_bridge_permit_join(uint8_t duration_s)
{
    (void)duration_s;
    return ESP_OK; // no-op stub on host
}

esp_err_t zigbee_bridge_poll_attribute(const uint8_t *ieee,
                                       uint16_t cluster, uint16_t attr)
{
    (void)cluster;
    (void)attr;
    return (ieee == NULL) ? ESP_ERR_INVALID_ARG : ESP_OK;
}

// --- injection helpers -------------------------------------------------------

void zigbee_bridge_mock_inject_frame(const uint8_t *raw, size_t len)
{
    zigbee_bridge_process_frame(raw, len);
}

void zigbee_bridge_mock_inject_sensor_report(const bridge_sensor_report_t *r)
{
    uint8_t frame[BRIDGE_MAX_FRAME];
    int flen = bridge_frame_encode(MSG_SENSOR_REPORT, (const uint8_t *)r,
                                   sizeof(*r), frame, sizeof(frame));
    if (flen > 0) {
        zigbee_bridge_mock_inject_frame(frame, (size_t)flen);
    }
}
