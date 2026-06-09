/**
 * @file hal_uart_mock.c
 * @brief Host mock of the UART HAL — in-memory loopback FIFO.
 *
 * Plain C, no ESP-IDF. Writes append to a fixed ring; reads consume in order.
 * Timeout is ignored (data is either present or not, synchronously).
 */
#include "hal_uart_mock.h"

#include <string.h>

#define HAL_UART_MOCK_CAP 2048

static uint8_t s_buf[HAL_UART_MOCK_CAP];
static size_t  s_head;  // next read index
static size_t  s_tail;  // next write index
static bool    s_inited;

esp_err_t hal_uart_init(void)
{
    s_inited = true;
    return ESP_OK;
}

int hal_uart_write(const uint8_t *data, size_t len)
{
    if (data == NULL && len > 0) return -1;
    if (!s_inited) return -1;
    size_t n = 0;
    for (; n < len && s_tail < HAL_UART_MOCK_CAP; n++) {
        s_buf[s_tail++] = data[n];
    }
    return (int)n; // bytes accepted (may be < len if FIFO full)
}

int hal_uart_read(uint8_t *buf, size_t len, uint32_t timeout_ms)
{
    (void)timeout_ms;
    if (buf == NULL) return -1;
    if (!s_inited) return -1;
    size_t n = 0;
    for (; n < len && s_head < s_tail; n++) {
        buf[n] = s_buf[s_head++];
    }
    return (int)n; // 0 when the FIFO is empty (mock "timeout")
}

void hal_uart_mock_reset(void)
{
    s_head = 0;
    s_tail = 0;
    s_inited = false;
    memset(s_buf, 0, sizeof(s_buf));
}

size_t hal_uart_mock_pending(void)
{
    return s_tail - s_head;
}
