/**
 * @file hal_uart_mstp_mock.c
 * @brief Host mock of the MS/TP UART HAL — in-memory loopback buffers.
 *
 * Plain C, no ESP-IDF. write() appends to a TX buffer (readable via
 * hal_uart_mstp_mock_get_tx); read() consumes from an RX buffer that tests fill
 * with hal_uart_mstp_mock_inject_rx. Direction is tracked but otherwise inert.
 */
#include "hal_uart_mstp.h"
#include "hal_uart_mstp_mock.h"

#include <string.h>

#define MSTP_MOCK_CAP 1024

static uint8_t s_rx[MSTP_MOCK_CAP];
static size_t  s_rx_head, s_rx_tail;
static uint8_t s_tx[MSTP_MOCK_CAP];
static size_t  s_tx_len;
static bool    s_transmit;

esp_err_t hal_uart_mstp_init(uint32_t baud)
{
    (void)baud;
    hal_uart_mstp_mock_reset();
    return ESP_OK;
}

esp_err_t hal_uart_mstp_set_direction(bool transmit)
{
    s_transmit = transmit;
    return ESP_OK;
}

esp_err_t hal_uart_mstp_write(const uint8_t *data, size_t len)
{
    if (data == NULL && len > 0) {
        return ESP_ERR_INVALID_ARG;
    }
    for (size_t i = 0; i < len && s_tx_len < MSTP_MOCK_CAP; i++) {
        s_tx[s_tx_len++] = data[i];
    }
    return ESP_OK;
}

esp_err_t hal_uart_mstp_read(uint8_t *data, size_t max_len, size_t *len,
                             uint32_t timeout_ms)
{
    (void)timeout_ms;
    if (data == NULL || len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t n = 0;
    while (n < max_len && s_rx_head < s_rx_tail) {
        data[n++] = s_rx[s_rx_head++];
    }
    *len = n;
    return ESP_OK;
}

void hal_uart_mstp_mock_inject_rx(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len && s_rx_tail < MSTP_MOCK_CAP; i++) {
        s_rx[s_rx_tail++] = data[i];
    }
}

size_t hal_uart_mstp_mock_get_tx(uint8_t *buf, size_t max)
{
    size_t n = (s_tx_len < max) ? s_tx_len : max;
    memcpy(buf, s_tx, n);
    return n;
}

void hal_uart_mstp_mock_reset(void)
{
    s_rx_head = s_rx_tail = 0;
    s_tx_len = 0;
    s_transmit = false;
}
