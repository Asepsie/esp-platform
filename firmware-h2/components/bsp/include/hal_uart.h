/**
 * @file hal_uart.h
 * @brief UART HAL (H2) — public interface for the C6↔H2 bridge link.
 *
 * Logical byte-stream access to the bridge UART. The HAL owns the port, pins,
 * baud, and ring buffers; callers (e.g. `uart_bridge.c`) only read/write bytes
 * and never include `driver/uart.h`. This is the H2 side of the HAL boundary
 * (hal-design.md): all `driver/uart.h` use lives in `hal_uart.c`.
 */
#ifndef HAL_UART_H
#define HAL_UART_H

#include <stddef.h>
#include <stdint.h>
#include "platform_compat.h"   /* esp_err_t (target: esp_err.h; host: shim) */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Pass as @p timeout_ms to @c hal_uart_read() to block indefinitely. */
#define HAL_UART_WAIT_FOREVER  0xFFFFFFFFu

/**
 * @brief Configure and start the bridge UART (115200 8N1, no flow control).
 * @return @c ESP_OK on success, or an @c esp_err_t from the driver.
 */
esp_err_t hal_uart_init(void);

/**
 * @brief Write @p len bytes (blocking until queued/sent).
 * @param data Source buffer (non-NULL unless @p len is 0).
 * @param len  Number of bytes to write.
 * @return Bytes written (== @p len on success), or -1 on error.
 */
int hal_uart_write(const uint8_t *data, size_t len);

/**
 * @brief Read up to @p len bytes, waiting at most @p timeout_ms.
 * @param[out] buf        Destination buffer (non-NULL).
 * @param[in]  len        Maximum bytes to read.
 * @param[in]  timeout_ms Max wait in ms, or @c HAL_UART_WAIT_FOREVER.
 * @return Bytes read (0 on timeout), or -1 on error.
 */
int hal_uart_read(uint8_t *buf, size_t len, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* HAL_UART_H */
