/**
 * @file hal_uart.c
 * @brief UART HAL — ESP32-C6 target implementation (bridge link to H2).
 *
 * The only file on the C6 that includes `driver/uart.h`. Owns UART1 setup
 * (port, pins from thermostat_config.h, baud, ring buffers) and exposes byte
 * read/write to the zigbee_bridge.
 */
#include "hal_uart.h"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"

#include "thermostat_config.h"  // UART_BRIDGE_TX_GPIO / _RX_GPIO / _BAUD

// UART1 — UART0 is the log console.
#define HAL_UART_PORT    UART_NUM_1
#define HAL_UART_RX_BUF  1024   // >= a few max frames
#define HAL_UART_TX_BUF  0      // 0 = blocking writes

esp_err_t hal_uart_init(void)
{
    const uart_config_t cfg = {
        .baud_rate  = UART_BRIDGE_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t err = uart_driver_install(HAL_UART_PORT, HAL_UART_RX_BUF,
                                        HAL_UART_TX_BUF, 0, NULL, 0);
    if (err != ESP_OK) return err;
    err = uart_param_config(HAL_UART_PORT, &cfg);
    if (err != ESP_OK) return err;
    return uart_set_pin(HAL_UART_PORT, UART_BRIDGE_TX_GPIO, UART_BRIDGE_RX_GPIO,
                        UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

int hal_uart_write(const uint8_t *data, size_t len)
{
    if (data == NULL && len > 0) {
        return -1;
    }
    return uart_write_bytes(HAL_UART_PORT, data, len);
}

int hal_uart_read(uint8_t *buf, size_t len, uint32_t timeout_ms)
{
    if (buf == NULL) {
        return -1;
    }
    const TickType_t ticks = (timeout_ms == HAL_UART_WAIT_FOREVER)
                                 ? portMAX_DELAY
                                 : pdMS_TO_TICKS(timeout_ms);
    return uart_read_bytes(HAL_UART_PORT, buf, len, ticks);
}
