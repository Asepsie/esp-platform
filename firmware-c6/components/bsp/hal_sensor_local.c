/**
 * @file hal_sensor_local.c
 * @brief SHT40 local sensor HAL — ESP32-C6 target implementation.
 *
 * Talks to the SHT40 over hal_i2c. A measurement is: write the high-precision
 * command, wait ~10 ms, read 6 bytes (T msb/lsb/crc, RH msb/lsb/crc). CRC is
 * not yet validated (future hardening). No driver headers here — I2C via hal_i2c.
 */
#include "hal_sensor_local.h"
#include "hal_i2c.h"
#include "hal_timer.h"
#include "hal_pin_map.h"   /* PINMAP_SHT40_ADDR */

#include <stdint.h>

#define SHT40_CMD_MEASURE_HIGH_PRECISION  0xFD
#define SHT40_MEASURE_DELAY_MS            10   /* high-precision ~8.3 ms + margin */

static bool s_available;

esp_err_t hal_sensor_local_read(float *temp_c, float *rh_pct)
{
    if (temp_c == NULL || rh_pct == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    const uint8_t cmd = SHT40_CMD_MEASURE_HIGH_PRECISION;
    esp_err_t err = hal_i2c_write(PINMAP_SHT40_ADDR, &cmd, 1);
    if (err != ESP_OK) {
        return err;
    }
    hal_timer_delay_ms(SHT40_MEASURE_DELAY_MS);

    uint8_t buf[6];
    err = hal_i2c_read(PINMAP_SHT40_ADDR, buf, sizeof(buf));
    if (err != ESP_OK) {
        return err;
    }

    const uint16_t t_raw  = ((uint16_t)buf[0] << 8) | buf[1];
    const uint16_t rh_raw = ((uint16_t)buf[3] << 8) | buf[4];

    // SHT40 datasheet conversions.
    float t  = -45.0f + 175.0f * ((float)t_raw / 65535.0f);
    float rh = -6.0f  + 125.0f * ((float)rh_raw / 65535.0f);
    if (rh < 0.0f)   rh = 0.0f;
    if (rh > 100.0f) rh = 100.0f;

    *temp_c = t;
    *rh_pct = rh;
    s_available = true;
    return ESP_OK;
}

esp_err_t hal_sensor_local_init(void)
{
    esp_err_t err = hal_i2c_init();
    if (err != ESP_OK) {
        return err;
    }
    // Probe by taking one reading; absence => sensor not fitted.
    float t, rh;
    s_available = (hal_sensor_local_read(&t, &rh) == ESP_OK);
    return s_available ? ESP_OK : ESP_ERR_NOT_FOUND;
}

bool hal_sensor_local_is_available(void)
{
    return s_available;
}
