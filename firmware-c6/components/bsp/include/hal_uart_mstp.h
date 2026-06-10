/**
 * @file hal_uart_mstp.h
 * @brief RS-485 UART HAL for BACnet MS/TP (C6) — public interface.
 *
 * Half-duplex RS-485 link (UART0 + a MAX485-class transceiver) used by the
 * BACnet MS/TP transport. The HAL owns the UART and the direction (DE) line;
 * the DE pin is driven through hal_gpio (HAL_GPIO_RS485_DE). Callers move only
 * bytes and never touch driver/uart.h. The MS/TP framing and token passing live
 * above this in the BACnet transport.
 *
 * @see docs/architecture/hal-design.md (HAL boundary, RS-485 drive circuit).
 */
#ifndef HAL_UART_MSTP_H
#define HAL_UART_MSTP_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "platform_compat.h"   /* esp_err_t */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configure and start the MS/TP RS-485 UART.
 *
 * Sets 8N1 at @p baud on the MS/TP UART/pins and leaves the line in receive
 * mode (DE low). Requires hal_gpio to be initialized (for the DE line).
 *
 * @param baud Line rate (typically 9600–76800; 38400 default).
 * @return @c ESP_OK on success, or an @c esp_err_t from the driver.
 */
esp_err_t hal_uart_mstp_init(uint32_t baud);

/**
 * @brief Write @p len bytes to the line (caller must be in transmit mode).
 * @param data Source buffer (non-NULL unless @p len is 0).
 * @param len  Number of bytes.
 * @return @c ESP_OK if all bytes were queued, else an @c esp_err_t.
 */
esp_err_t hal_uart_mstp_write(const uint8_t *data, size_t len);

/**
 * @brief Read up to @p max_len bytes, waiting at most @p timeout_ms.
 * @param[out] data    Destination buffer (non-NULL).
 * @param[in]  max_len Buffer capacity.
 * @param[out] len     Receives the number of bytes read (0 on timeout).
 * @param[in]  timeout_ms Max wait in milliseconds.
 * @return @c ESP_OK on success (including a 0-length timeout), else @c esp_err_t.
 */
esp_err_t hal_uart_mstp_read(uint8_t *data, size_t max_len, size_t *len,
                             uint32_t timeout_ms);

/**
 * @brief Set the RS-485 transceiver direction.
 * @param transmit @c true → DE high (drive the bus, TX); @c false → DE low (RX).
 * @return @c ESP_OK on success, or an @c esp_err_t from the GPIO HAL.
 */
esp_err_t hal_uart_mstp_set_direction(bool transmit);

#ifdef __cplusplus
}
#endif

#endif /* HAL_UART_MSTP_H */
