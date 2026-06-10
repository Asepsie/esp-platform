/**
 * @file hal_i2c_expander_mock.h
 * @brief Host mock for the I/O expander HAL — inspect/seed chip state.
 *
 * Models a single instance of each chip type (addresses are ignored), which is
 * sufficient for io_scan unit tests: seed inputs / ADC results, read back what
 * was written to outputs / the DAC.
 */
#ifndef HAL_I2C_EXPANDER_MOCK_H
#define HAL_I2C_EXPANDER_MOCK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Reset all mocked expander state (call from a test's setUp()). */
void hal_i2c_expander_mock_reset(void);

/* MCP23017 */
/** @brief Seed an input port (0 = A, 1 = B) value the chip will report. */
void hal_mcp23017_mock_set_input(uint8_t port, uint8_t value);
/** @brief Read back the value last written to an output port (0 = A, 1 = B). */
uint8_t hal_mcp23017_mock_get_output(uint8_t port);

/* ADS1115 */
/** @brief Seed the raw result for a channel (0–3). */
void hal_ads1115_mock_set_raw(uint8_t channel, int16_t raw);

/* MCP4728 */
/** @brief Read back the 12-bit value last written to a DAC channel (0–3). */
uint16_t hal_mcp4728_mock_get_channel(uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif /* HAL_I2C_EXPANDER_MOCK_H */
