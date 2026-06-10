/**
 * @file bacnet_transport_mstp.c
 * @brief BACnet MS/TP transport — STUB datalink over hal_uart_mstp.
 *
 * Wires the transport ops to the RS-485 HAL so the interface is exercised end
 * to end. MS/TP framing (preamble, frame types, CRC) and token passing are
 * future work — send/receive currently move raw bytes. Includes ONLY
 * hal_uart_mstp.h (no driver headers — HAL boundary).
 */
#include "bacnet_transport.h"
#include "hal_uart_mstp.h"
#include "thermostat_config.h"   /* MSTP_BAUD_DEFAULT, MSTP_MAC_ADDRESS */

#include <string.h>

static esp_err_t mstp_init(void)
{
    return hal_uart_mstp_init(MSTP_BAUD_DEFAULT);
}

static esp_err_t mstp_send(const uint8_t *pdu, size_t len, bacnet_addr_t *dest)
{
    (void)dest; // TODO: MS/TP addressing + framing/token passing
    esp_err_t err = hal_uart_mstp_set_direction(true); // drive the bus
    if (err == ESP_OK) {
        err = hal_uart_mstp_write(pdu, len);
    }
    (void)hal_uart_mstp_set_direction(false); // back to receive
    return err;
}

static esp_err_t mstp_receive(uint8_t *pdu, size_t max_len, size_t *len,
                              bacnet_addr_t *src, uint32_t timeout_ms)
{
    if (src != NULL) {
        src->net = 0;
        src->mac_len = 1;
        src->mac[0] = MSTP_MAC_ADDRESS; // placeholder until framing extracts it
    }
    return hal_uart_mstp_read(pdu, max_len, len, timeout_ms);
}

static void mstp_cleanup(void) { /* nothing to release in the stub */ }

static const bacnet_transport_ops_t s_mstp_ops = {
    .init    = mstp_init,
    .send    = mstp_send,
    .receive = mstp_receive,
    .cleanup = mstp_cleanup,
};

const bacnet_transport_ops_t *bacnet_transport_mstp_ops(void)
{
    return &s_mstp_ops;
}
