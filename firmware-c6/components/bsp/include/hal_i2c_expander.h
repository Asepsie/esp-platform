/**
 * @file hal_i2c_expander.h
 * @brief I2C I/O expander drivers (C6): MCP23017, ADS1115, MCP4728.
 *
 * Thin drivers over hal_i2c for the wired-I/O expansion chips used by io_scan.
 * MCP23017 = 16 digital I/O, ADS1115 = 4-channel 16-bit ADC, MCP4728 = 4-channel
 * 12-bit DAC. Callers never include driver/i2c_master.h. A single-instance host
 * mock backs all three for unit tests.
 */
#ifndef HAL_I2C_EXPANDER_H
#define HAL_I2C_EXPANDER_H

#include <stdint.h>
#include <stdbool.h>
#include "platform_compat.h"   /* esp_err_t */

#ifdef __cplusplus
extern "C" {
#endif

/* ===== MCP23017 — 16-bit digital I/O ===================================== */

/** @brief Initialize an MCP23017 at @p i2c_addr. */
esp_err_t hal_mcp23017_init(uint8_t i2c_addr);
/** @brief Read an 8-bit port (@p port: 0 = A, 1 = B) into @p value. */
esp_err_t hal_mcp23017_read_port(uint8_t addr, uint8_t port, uint8_t *value);
/** @brief Write an 8-bit port (@p port: 0 = A, 1 = B). */
esp_err_t hal_mcp23017_write_port(uint8_t addr, uint8_t port, uint8_t value);
/** @brief Configure one pin (0–15) as input (@p is_input true) or output. */
esp_err_t hal_mcp23017_configure_pin(uint8_t addr, uint8_t pin, bool is_input);

/* ===== ADS1115 — 4-channel 16-bit ADC ==================================== */

/** @brief ADS1115 programmable-gain settings (full-scale range). */
typedef enum {
    ADS1115_GAIN_6V144 = 0, /**< ±6.144 V */
    ADS1115_GAIN_4V096,     /**< ±4.096 V */
    ADS1115_GAIN_2V048,     /**< ±2.048 V (default) */
    ADS1115_GAIN_1V024,     /**< ±1.024 V */
    ADS1115_GAIN_0V512,     /**< ±0.512 V */
    ADS1115_GAIN_0V256,     /**< ±0.256 V */
} ads1115_gain_t;

/** @brief Initialize an ADS1115 at @p i2c_addr. */
esp_err_t hal_ads1115_init(uint8_t i2c_addr);
/** @brief Start a single-ended conversion on @p channel (0–3). */
esp_err_t hal_ads1115_start_conversion(uint8_t addr, uint8_t channel);
/** @brief Read the last completed conversion result (signed raw). */
esp_err_t hal_ads1115_read_result(uint8_t addr, int16_t *raw_value);
/** @brief Convert a raw ADS1115 reading to volts for the given gain. */
float hal_ads1115_raw_to_voltage(int16_t raw, ads1115_gain_t gain);

/* ===== MCP4728 — 4-channel 12-bit DAC ==================================== */

/** @brief Initialize an MCP4728 at @p i2c_addr. */
esp_err_t hal_mcp4728_init(uint8_t i2c_addr);
/** @brief Write a 12-bit @p value (0–4095) to DAC @p channel (0–3). */
esp_err_t hal_mcp4728_write_channel(uint8_t addr, uint8_t channel, uint16_t value);

#ifdef __cplusplus
}
#endif

#endif /* HAL_I2C_EXPANDER_H */
