/**
 * @file hal_i2c_expander_mock.c
 * @brief Host mock of the I/O expander HAL — single-instance per chip type.
 *
 * Plain C, no ESP-IDF. Addresses are ignored (one virtual chip each). The ADC
 * mock returns the raw for the channel whose conversion was last "started",
 * mirroring the start→read pipeline io_scan uses.
 */
#include "hal_i2c_expander.h"
#include "hal_i2c_expander_mock.h"

/* MCP23017 */
static uint8_t s_mcp_in[2];
static uint8_t s_mcp_out[2];

/* ADS1115 */
static int16_t s_ads_raw[4];
static uint8_t s_ads_current;

/* MCP4728 */
static uint16_t s_dac[4];

esp_err_t hal_mcp23017_init(uint8_t i2c_addr) { (void)i2c_addr; return ESP_OK; }
esp_err_t hal_mcp23017_configure_pin(uint8_t addr, uint8_t pin, bool is_input)
{
    (void)addr; (void)pin; (void)is_input; return ESP_OK;
}
esp_err_t hal_mcp23017_read_port(uint8_t addr, uint8_t port, uint8_t *value)
{
    (void)addr;
    if (value == NULL) return ESP_ERR_INVALID_ARG;
    *value = s_mcp_in[port & 1];
    return ESP_OK;
}
esp_err_t hal_mcp23017_write_port(uint8_t addr, uint8_t port, uint8_t value)
{
    (void)addr;
    s_mcp_out[port & 1] = value;
    return ESP_OK;
}

esp_err_t hal_ads1115_init(uint8_t i2c_addr) { (void)i2c_addr; return ESP_OK; }
esp_err_t hal_ads1115_start_conversion(uint8_t addr, uint8_t channel)
{
    (void)addr;
    s_ads_current = channel & 0x3;
    return ESP_OK;
}
esp_err_t hal_ads1115_read_result(uint8_t addr, int16_t *raw_value)
{
    (void)addr;
    if (raw_value == NULL) return ESP_ERR_INVALID_ARG;
    *raw_value = s_ads_raw[s_ads_current];
    return ESP_OK;
}
float hal_ads1115_raw_to_voltage(int16_t raw, ads1115_gain_t gain)
{
    static const float fs[] = { 6.144f, 4.096f, 2.048f, 1.024f, 0.512f, 0.256f };
    float f = (gain <= ADS1115_GAIN_0V256) ? fs[gain] : fs[2];
    return ((float)raw / 32768.0f) * f;
}

esp_err_t hal_mcp4728_init(uint8_t i2c_addr) { (void)i2c_addr; return ESP_OK; }
esp_err_t hal_mcp4728_write_channel(uint8_t addr, uint8_t channel, uint16_t value)
{
    (void)addr;
    s_dac[channel & 0x3] = value & 0x0FFF;
    return ESP_OK;
}

/* --- inspection helpers ------------------------------------------------- */

void hal_i2c_expander_mock_reset(void)
{
    for (int i = 0; i < 2; i++) { s_mcp_in[i] = 0; s_mcp_out[i] = 0; }
    for (int i = 0; i < 4; i++) { s_ads_raw[i] = 0; s_dac[i] = 0; }
    s_ads_current = 0;
}

void hal_mcp23017_mock_set_input(uint8_t port, uint8_t value) { s_mcp_in[port & 1] = value; }
uint8_t hal_mcp23017_mock_get_output(uint8_t port) { return s_mcp_out[port & 1]; }
void hal_ads1115_mock_set_raw(uint8_t channel, int16_t raw) { s_ads_raw[channel & 0x3] = raw; }
uint16_t hal_mcp4728_mock_get_channel(uint8_t channel) { return s_dac[channel & 0x3]; }
