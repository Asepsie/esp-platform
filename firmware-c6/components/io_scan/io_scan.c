/**
 * @file io_scan.c
 * @brief Pipelined wired-I/O scan implementation.
 *
 * Phases per tick (see io_scan.h): (1) read DI, (2/4) read previous ADC result +
 * start next channel, (5) write DO + AO, (6) read SHT40 every Nth tick → store.
 * Talks only to the HALs (hal_i2c_expander, hal_sensor_local, hal_timer) so it
 * is host-testable against their mocks. Chip presence is config-gated by the
 * IO_*_COUNT macros (0 disables; test builds override via -D).
 */
#include "io_scan.h"
#include "hal_i2c_expander.h"
#include "hal_sensor_local.h"
#include "hal_timer.h"
#include "sensor_state.h"
#include "thermostat_config.h"

#define IO_MAX_AI_CHANNELS  8           /* up to 2 ADS1115 × 4 */
#define IO_AO_FULL_SCALE_V  3.3f        /* MCP4728 reference (VDD) */
#define IO_ADS1115_GAIN     ADS1115_GAIN_2V048
#define IO_MCP23017_PORT_DI 0           /* port A = inputs  */
#define IO_MCP23017_PORT_DO 1           /* port B = outputs */

static uint8_t  s_di;                   /* last DI image (8 bits)   */
static uint8_t  s_do;                   /* DO image to write        */
static int16_t  s_ai_raw[IO_MAX_AI_CHANNELS];
static float    s_ai_volt[IO_MAX_AI_CHANNELS];
static uint16_t s_ao[4];                /* DAC codes to write       */
static uint8_t  s_adc_ch;               /* round-robin AI channel   */
static uint32_t s_tick;
static uint32_t s_last_duration_us;

static uint8_t ai_channel_count(void)
{
    uint8_t n = (uint8_t)(IO_ADS1115_COUNT * 4);
    return (n > IO_MAX_AI_CHANNELS) ? IO_MAX_AI_CHANNELS : n;
}

// I2C address of the ADS1115 carrying global AI channel `ch` (4 channels/chip).
static uint8_t ads_addr(uint8_t ch)
{
    return (ch < 4) ? IO_ADS1115_ADDR_1 : IO_ADS1115_ADDR_2;
}

esp_err_t io_scan_init(void)
{
    // Brings up the shared I2C bus on target; the SHT40 itself is optional.
    (void)hal_sensor_local_init();

    for (int i = 0; i < IO_MCP23017_COUNT; i++) {
        uint8_t addr = (i == 0) ? IO_MCP23017_ADDR_1 : IO_MCP23017_ADDR_2;
        hal_mcp23017_init(addr);
        for (uint8_t p = 0; p < 8; p++)  hal_mcp23017_configure_pin(addr, p, true);       // A in
        for (uint8_t p = 8; p < 16; p++) hal_mcp23017_configure_pin(addr, p, false);      // B out
    }
    for (int i = 0; i < IO_ADS1115_COUNT; i++) {
        hal_ads1115_init((i == 0) ? IO_ADS1115_ADDR_1 : IO_ADS1115_ADDR_2);
    }
    if (IO_MCP4728_COUNT > 0) {
        hal_mcp4728_init(IO_MCP4728_ADDR);
    }

    s_di = 0; s_do = 0; s_adc_ch = 0; s_tick = 0; s_last_duration_us = 0;
    for (int i = 0; i < IO_MAX_AI_CHANNELS; i++) { s_ai_raw[i] = 0; s_ai_volt[i] = 0.0f; }
    for (int i = 0; i < 4; i++) s_ao[i] = 0;

    // Kick off the first conversion so tick 1 has a result to read.
    if (ai_channel_count() > 0) {
        hal_ads1115_start_conversion(ads_addr(0), 0);
    }
    return ESP_OK;
}

esp_err_t io_scan_safety_interrupt(void)
{
    if (IO_MCP23017_COUNT == 0) {
        return ESP_OK;
    }
    uint8_t v;
    esp_err_t err = hal_mcp23017_read_port(IO_MCP23017_ADDR_1, IO_MCP23017_PORT_DI, &v);
    if (err == ESP_OK) {
        s_di = v; // refresh DI immediately (safety path)
    }
    return err;
}

esp_err_t io_scan_tick(void)
{
    const uint32_t t0 = hal_timer_get_us();
    s_tick++;

    // Phase 1 — digital inputs.
    if (IO_MCP23017_COUNT > 0) {
        uint8_t v;
        if (hal_mcp23017_read_port(IO_MCP23017_ADDR_1, IO_MCP23017_PORT_DI, &v) == ESP_OK) {
            s_di = v;
        }
    }

    // Phases 2/4 — read the previous cycle's ADC result, start the next channel.
    const uint8_t n_ai = ai_channel_count();
    if (n_ai > 0) {
        int16_t raw;
        if (hal_ads1115_read_result(ads_addr(s_adc_ch), &raw) == ESP_OK) {
            s_ai_raw[s_adc_ch]  = raw;
            s_ai_volt[s_adc_ch] = hal_ads1115_raw_to_voltage(raw, IO_ADS1115_GAIN);
        }
        s_adc_ch = (uint8_t)((s_adc_ch + 1) % n_ai);
        hal_ads1115_start_conversion(ads_addr(s_adc_ch), (uint8_t)(s_adc_ch % 4));
    }

    // Phase 5 — write outputs.
    if (IO_MCP23017_COUNT > 0) {
        hal_mcp23017_write_port(IO_MCP23017_ADDR_1, IO_MCP23017_PORT_DO, s_do);
    }
    if (IO_MCP4728_COUNT > 0) {
        for (uint8_t ch = 0; ch < 4; ch++) {
            hal_mcp4728_write_channel(IO_MCP4728_ADDR, ch, s_ao[ch]);
        }
    }

    // Phase 6 — local SHT40 on a slow cadence → state store (fallback source).
    if ((s_tick % IO_SCAN_SHT40_INTERVAL) == 0) {
        float t, rh;
        if (hal_sensor_local_read(&t, &rh) == ESP_OK) {
            (void)sensor_state_update_local(t, rh);
        }
    }

    s_last_duration_us = hal_timer_get_us() - t0;
    return ESP_OK;
}

esp_err_t io_scan_get_di(uint8_t port, bool *value)
{
    if (port >= 8 || value == NULL) return ESP_ERR_INVALID_ARG;
    *value = ((s_di >> port) & 1u) != 0;
    return ESP_OK;
}

esp_err_t io_scan_get_ai(uint8_t channel, float *value)
{
    if (channel >= IO_MAX_AI_CHANNELS || value == NULL) return ESP_ERR_INVALID_ARG;
    *value = s_ai_volt[channel];
    return ESP_OK;
}

esp_err_t io_scan_get_ai_raw(uint8_t channel, int16_t *raw)
{
    if (channel >= IO_MAX_AI_CHANNELS || raw == NULL) return ESP_ERR_INVALID_ARG;
    *raw = s_ai_raw[channel];
    return ESP_OK;
}

esp_err_t io_scan_set_do(uint8_t port, bool value)
{
    if (port >= 8) return ESP_ERR_INVALID_ARG;
    if (value) s_do |= (uint8_t)(1u << port);
    else       s_do &= (uint8_t)~(1u << port);
    return ESP_OK;
}

esp_err_t io_scan_set_ao(uint8_t channel, float voltage)
{
    if (channel >= 4) return ESP_ERR_INVALID_ARG;
    if (voltage < 0.0f) voltage = 0.0f;
    float code = (voltage / IO_AO_FULL_SCALE_V) * 4095.0f;
    if (code > 4095.0f) code = 4095.0f;
    s_ao[channel] = (uint16_t)code;
    return ESP_OK;
}

uint32_t io_scan_get_last_duration_us(void)
{
    return s_last_duration_us;
}
