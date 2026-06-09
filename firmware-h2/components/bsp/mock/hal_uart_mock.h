/**
 * @file hal_uart_mock.h
 * @brief Host mock for the UART HAL — in-memory loopback + helpers.
 *
 * The mock (@c hal_uart_mock.c) implements @c hal_uart.h as a byte FIFO:
 * @c hal_uart_write() appends, @c hal_uart_read() consumes. This makes it a
 * loopback, so bridge framing can be exercised on host without hardware.
 */
#ifndef HAL_UART_MOCK_H
#define HAL_UART_MOCK_H

#include "hal_uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Clear the loopback FIFO and reset init state (call from setUp()). */
void hal_uart_mock_reset(void);

/** @brief Number of bytes currently buffered (written but not yet read). */
size_t hal_uart_mock_pending(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_UART_MOCK_H */
