/**
 * @file hal_i2c.c
 * @brief I2C master HAL — ESP32-C6 target implementation (new i2c_master API).
 *
 * Owns the shared expansion bus and a small lazily-populated table of device
 * handles (one per 7-bit address). Sole includer of driver/i2c_master.h.
 */
#include "hal_i2c.h"

#include "driver/i2c_master.h"
#include "thermostat_config.h"   /* PINMAP_I2C_EXPANSION_SDA/SCL, I2C_EXPANSION_FREQ_HZ */

#define HAL_I2C_TIMEOUT_MS  100
#define HAL_I2C_MAX_DEVS    8

static i2c_master_bus_handle_t s_bus;
static bool                    s_inited;

static struct {
    uint8_t                  addr;
    i2c_master_dev_handle_t  handle;
} s_devs[HAL_I2C_MAX_DEVS];
static int s_dev_count;

esp_err_t hal_i2c_init(void)
{
    if (s_inited) {
        return ESP_OK;
    }
    const i2c_master_bus_config_t cfg = {
        .i2c_port = -1,                       // auto-select a free port
        .sda_io_num = PINMAP_I2C_EXPANSION_SDA,
        .scl_io_num = PINMAP_I2C_EXPANSION_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&cfg, &s_bus);
    if (err != ESP_OK) {
        return err;
    }
    s_inited = true;
    return ESP_OK;
}

// Return (creating if needed) the device handle for a 7-bit address.
static i2c_master_dev_handle_t get_dev(uint8_t addr)
{
    for (int i = 0; i < s_dev_count; i++) {
        if (s_devs[i].addr == addr) {
            return s_devs[i].handle;
        }
    }
    if (s_dev_count >= HAL_I2C_MAX_DEVS) {
        return NULL;
    }
    const i2c_device_config_t dcfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = addr,
        .scl_speed_hz    = I2C_EXPANSION_FREQ_HZ,
    };
    i2c_master_dev_handle_t h;
    if (i2c_master_bus_add_device(s_bus, &dcfg, &h) != ESP_OK) {
        return NULL;
    }
    s_devs[s_dev_count].addr = addr;
    s_devs[s_dev_count].handle = h;
    s_dev_count++;
    return h;
}

esp_err_t hal_i2c_write(uint8_t addr, const uint8_t *data, size_t len)
{
    if (!s_inited) return ESP_ERR_INVALID_STATE;
    i2c_master_dev_handle_t h = get_dev(addr);
    if (h == NULL) return ESP_FAIL;
    return i2c_master_transmit(h, data, len, HAL_I2C_TIMEOUT_MS);
}

esp_err_t hal_i2c_read(uint8_t addr, uint8_t *data, size_t len)
{
    if (!s_inited) return ESP_ERR_INVALID_STATE;
    i2c_master_dev_handle_t h = get_dev(addr);
    if (h == NULL) return ESP_FAIL;
    return i2c_master_receive(h, data, len, HAL_I2C_TIMEOUT_MS);
}
