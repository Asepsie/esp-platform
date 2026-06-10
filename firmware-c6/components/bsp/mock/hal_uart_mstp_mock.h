/**
 * @file hal_uart_mstp_mock.h
 * @brief Host mock for the MS/TP UART HAL — loopback inspection helpers.
 *
 * The mock (@c hal_uart_mstp_mock.c) backs @c hal_uart_mstp.h with in-memory
 * RX/TX buffers so the MS/TP transport can be exercised on host: tests inject
 * bytes "from the bus" and read back what the transport transmitted.
 */
#ifndef HAL_UART_MSTP_MOCK_H
#define HAL_UART_MSTP_MOCK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Feed bytes into the RX buffer as if received from the bus. */
void hal_uart_mstp_mock_inject_rx(const uint8_t *data, size_t len);

/**
 * @brief Copy out bytes the HAL has transmitted (up to @p max).
 * @return Number of bytes copied.
 */
size_t hal_uart_mstp_mock_get_tx(uint8_t *buf, size_t max);

/** @brief Clear RX/TX buffers and state (call from a test's setUp()). */
void hal_uart_mstp_mock_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_UART_MSTP_MOCK_H */
