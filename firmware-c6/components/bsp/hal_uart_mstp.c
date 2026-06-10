/**
 * @file hal_uart_mstp.c
 * @brief RS-485 UART HAL for BACnet MS/TP — ESP32-C6 target implementation.
 *
 * Wraps driver/uart.h for the MS/TP UART (pins/port from hal_pin_map.h) and
 * drives the RS-485 DE line through hal_gpio (HAL_GPIO_RS485_DE). Sole includer
 * of driver/uart.h for this peripheral.
 */
#include "hal_uart_mstp.h"
#include "hal_gpio.h"
#include "hal_pin_map.h"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"

#define MSTP_RX_BUF  512   // driver ring buffer
#define MSTP_TX_BUF  0     // 0 = blocking writes

esp_err_t hal_uart_mstp_set_direction(bool transmit)
{
    // DE+RE tied together: high → transmit (drive bus), low → receive.
    return hal_gpio_set(HAL_GPIO_RS485_DE, transmit);
}

esp_err_t hal_uart_mstp_init(uint32_t baud)
{
    const uart_config_t cfg = {
        .baud_rate  = (int)baud,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t err = uart_driver_install(PINMAP_MSTP_UART, MSTP_RX_BUF,
                                        MSTP_TX_BUF, 0, NULL, 0);
    if (err != ESP_OK) return err;
    err = uart_param_config(PINMAP_MSTP_UART, &cfg);
    if (err != ESP_OK) return err;
    err = uart_set_pin(PINMAP_MSTP_UART, PINMAP_MSTP_TX, PINMAP_MSTP_RX,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return err;
    return hal_uart_mstp_set_direction(false); // idle in receive
}

esp_err_t hal_uart_mstp_write(const uint8_t *data, size_t len)
{
    if (data == NULL && len > 0) {
        return ESP_ERR_INVALID_ARG;
    }
    int written = uart_write_bytes(PINMAP_MSTP_UART, data, len);
    return (written == (int)len) ? ESP_OK : ESP_FAIL;
}

esp_err_t hal_uart_mstp_read(uint8_t *data, size_t max_len, size_t *len,
                             uint32_t timeout_ms)
{
    if (data == NULL || len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    int r = uart_read_bytes(PINMAP_MSTP_UART, data, max_len,
                            pdMS_TO_TICKS(timeout_ms));
    if (r < 0) {
        return ESP_FAIL;
    }
    *len = (size_t)r;
    return ESP_OK;
}
