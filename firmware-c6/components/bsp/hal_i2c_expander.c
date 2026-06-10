/**
 * @file hal_i2c_expander.c
 * @brief I2C I/O expander drivers — ESP32-C6 target implementation.
 *
 * MCP23017 / ADS1115 / MCP4728 over hal_i2c (no driver headers here). Register
 * maps per each chip's datasheet; conversions are single-ended for the ADC.
 */
#include "hal_i2c_expander.h"
#include "hal_i2c.h"

/* ===== MCP23017 ========================================================== */
#define MCP23017_REG_IODIRA  0x00
#define MCP23017_REG_GPIOA   0x12
#define MCP23017_REG_OLATA   0x14

esp_err_t hal_mcp23017_init(uint8_t i2c_addr)
{
    (void)i2c_addr; // defaults (all inputs) are fine until pins are configured
    return ESP_OK;
}

esp_err_t hal_mcp23017_configure_pin(uint8_t addr, uint8_t pin, bool is_input)
{
    const uint8_t reg = MCP23017_REG_IODIRA + (pin / 8); // IODIRA / IODIRB
    const uint8_t bit = pin % 8;
    uint8_t dir;
    uint8_t r = reg;
    esp_err_t err = hal_i2c_write(addr, &r, 1);
    if (err != ESP_OK) return err;
    err = hal_i2c_read(addr, &dir, 1);
    if (err != ESP_OK) return err;
    dir = is_input ? (uint8_t)(dir | (1u << bit)) : (uint8_t)(dir & ~(1u << bit));
    const uint8_t out[2] = { reg, dir };
    return hal_i2c_write(addr, out, sizeof(out));
}

esp_err_t hal_mcp23017_read_port(uint8_t addr, uint8_t port, uint8_t *value)
{
    if (value == NULL) return ESP_ERR_INVALID_ARG;
    const uint8_t reg = MCP23017_REG_GPIOA + (port & 1);
    esp_err_t err = hal_i2c_write(addr, &reg, 1);
    if (err != ESP_OK) return err;
    return hal_i2c_read(addr, value, 1);
}

esp_err_t hal_mcp23017_write_port(uint8_t addr, uint8_t port, uint8_t value)
{
    const uint8_t out[2] = { (uint8_t)(MCP23017_REG_OLATA + (port & 1)), value };
    return hal_i2c_write(addr, out, sizeof(out));
}

/* ===== ADS1115 =========================================================== */
#define ADS1115_REG_CONVERSION  0x00
#define ADS1115_REG_CONFIG      0x01

// Full-scale volts per gain setting, indexed by ads1115_gain_t.
static const float ADS1115_FS[] = { 6.144f, 4.096f, 2.048f, 1.024f, 0.512f, 0.256f };

esp_err_t hal_ads1115_init(uint8_t i2c_addr)
{
    (void)i2c_addr;
    return ESP_OK;
}

esp_err_t hal_ads1115_start_conversion(uint8_t addr, uint8_t channel)
{
    // OS=1 (start), MUX=100+channel (single-ended AINx), PGA=±2.048V, single-shot,
    // 128 SPS. Config is a 16-bit big-endian register.
    const uint16_t cfg = 0x8000 | (uint16_t)((0x4 | (channel & 0x3)) << 12) |
                         (0x2 << 9) | (0x1 << 8) | 0x0083;
    const uint8_t out[3] = { ADS1115_REG_CONFIG, (uint8_t)(cfg >> 8), (uint8_t)cfg };
    return hal_i2c_write(addr, out, sizeof(out));
}

esp_err_t hal_ads1115_read_result(uint8_t addr, int16_t *raw_value)
{
    if (raw_value == NULL) return ESP_ERR_INVALID_ARG;
    const uint8_t reg = ADS1115_REG_CONVERSION;
    esp_err_t err = hal_i2c_write(addr, &reg, 1);
    if (err != ESP_OK) return err;
    uint8_t buf[2];
    err = hal_i2c_read(addr, buf, sizeof(buf));
    if (err != ESP_OK) return err;
    *raw_value = (int16_t)(((uint16_t)buf[0] << 8) | buf[1]);
    return ESP_OK;
}

float hal_ads1115_raw_to_voltage(int16_t raw, ads1115_gain_t gain)
{
    float fs = (gain <= ADS1115_GAIN_0V256) ? ADS1115_FS[gain] : ADS1115_FS[2];
    return ((float)raw / 32768.0f) * fs;
}

/* ===== MCP4728 =========================================================== */

esp_err_t hal_mcp4728_init(uint8_t i2c_addr)
{
    (void)i2c_addr;
    return ESP_OK;
}

esp_err_t hal_mcp4728_write_channel(uint8_t addr, uint8_t channel, uint16_t value)
{
    // Multi-write command (0x40) | channel<<1; then 12-bit value (VREF=VDD).
    value &= 0x0FFF;
    const uint8_t out[3] = {
        (uint8_t)(0x40 | ((channel & 0x3) << 1)),
        (uint8_t)(value >> 8),    // upper nibble (VREF/PD/Gain bits left 0)
        (uint8_t)(value & 0xFF),
    };
    return hal_i2c_write(addr, out, sizeof(out));
}
