/**
 * @file io_scan.h
 * @brief Wired-I/O scan — runs inside the control loop tick.
 *
 * Pipelined scan of the optional I2C I/O expanders (MCP23017 DI/DO, ADS1115 AI,
 * MCP4728 AO) plus the onboard SHT40. Per tick: read DI, read the PREVIOUS
 * cycle's ADC result and start the next channel, write DO/AO, and read the SHT40
 * every Nth tick. Analog inputs are therefore one cycle (≈1 s) stale — a
 * deliberate, documented choice: HVAC thermal time constants are minutes, so it
 * is irrelevant. Read/write accessors operate on the last scanned image.
 *
 * @see docs/architecture/rt-rules-v2.md (io_scan budget; AI staleness note).
 */
#ifndef IO_SCAN_H
#define IO_SCAN_H

#include <stdint.h>
#include <stdbool.h>
#include "platform_compat.h"   /* esp_err_t */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the I/O scan: bring up configured expanders + local sensor.
 * @return @c ESP_OK (the local SHT40 is optional; its absence is not an error).
 */
esp_err_t io_scan_init(void);

/**
 * @brief Run one pipelined scan cycle. Call once per control loop tick.
 * @return @c ESP_OK (individual I/O errors are absorbed; last image is kept).
 */
esp_err_t io_scan_tick(void);

/**
 * @brief Immediate digital-input read, for the safety-interrupt fast path.
 *
 * Called from the MCP23017 INT (GPIO14) ISR context to refresh DI without
 * waiting for the next scan tick (<1 ms response).
 * @return @c ESP_OK or an @c esp_err_t from the expander.
 */
esp_err_t io_scan_safety_interrupt(void);

/** @brief Read the last scanned digital input @p port (0–7). */
esp_err_t io_scan_get_di(uint8_t port, bool *value);
/** @brief Read the last scanned analog input @p channel (volts). */
esp_err_t io_scan_get_ai(uint8_t channel, float *value);
/** @brief Read the last scanned analog input @p channel (raw signed code). */
esp_err_t io_scan_get_ai_raw(uint8_t channel, int16_t *raw);

/** @brief Set a digital output @p port (0–7); applied on the next scan tick. */
esp_err_t io_scan_set_do(uint8_t port, bool value);
/** @brief Set an analog output @p channel (0–3) to @p voltage; applied next tick. */
esp_err_t io_scan_set_ao(uint8_t channel, float voltage);

/**
 * @brief Duration of the last scan cycle, microseconds.
 * @note Exposed northbound as BACnet Analog Input instance 304 (Diag-IOScanTime).
 */
uint32_t io_scan_get_last_duration_us(void);

#ifdef __cplusplus
}
#endif

#endif /* IO_SCAN_H */
