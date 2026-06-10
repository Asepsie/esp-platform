/**
 * @file hal_sensor_local_mock.c
 * @brief Host mock of the local SHT40 sensor HAL — configurable values.
 *
 * Plain C, no ESP-IDF/I2C. Tests set the returned temperature/RH and presence.
 */
#include "hal_sensor_local.h"
#include "hal_sensor_local_mock.h"

#include "platform_compat.h"   /* esp_err_t */

static float    s_temp = 20.0f;
static float    s_rh   = 50.0f;
static bool     s_available;
static unsigned s_read_count;

esp_err_t hal_sensor_local_init(void)
{
    s_available = true;
    return ESP_OK;
}

esp_err_t hal_sensor_local_read(float *temp_c, float *rh_pct)
{
    if (temp_c == NULL || rh_pct == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_available) {
        return ESP_ERR_INVALID_STATE;
    }
    s_read_count++;
    *temp_c = s_temp;
    *rh_pct = s_rh;
    return ESP_OK;
}

unsigned hal_sensor_local_mock_read_count(void)
{
    return s_read_count;
}

void hal_sensor_local_mock_reset(void)
{
    s_read_count = 0;
    s_available = false;
}

bool hal_sensor_local_is_available(void)
{
    return s_available;
}

void hal_sensor_local_mock_set(float temp, float rh)
{
    s_temp = temp;
    s_rh = rh;
    s_available = true;
}

void hal_sensor_local_mock_set_available(bool available)
{
    s_available = available;
}
