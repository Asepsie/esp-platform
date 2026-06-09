/**
 * @file hal_nvs.c
 * @brief NVS storage HAL — ESP32-C6 target implementation.
 *
 * Wraps ESP-IDF NVS. Sole owner of the HAL namespace handle. Missing keys are
 * normalized to @c ESP_ERR_NOT_FOUND so callers (and the host mock) see one
 * code regardless of backend.
 */
#include "hal_nvs.h"

#include "nvs_flash.h"
#include "nvs.h"

#define HAL_NVS_NAMESPACE "hal_kv"

static nvs_handle_t s_handle;
static bool         s_inited;

// Normalize backend "not found" to the portable code used across the HAL.
static esp_err_t xlate(esp_err_t err)
{
    return (err == ESP_ERR_NVS_NOT_FOUND) ? ESP_ERR_NOT_FOUND : err;
}

esp_err_t hal_nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        err = nvs_flash_erase();
        if (err == ESP_OK) {
            err = nvs_flash_init();
        }
    }
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_open(HAL_NVS_NAMESPACE, NVS_READWRITE, &s_handle);
    if (err != ESP_OK) {
        return err;
    }
    s_inited = true;
    return ESP_OK;
}

esp_err_t hal_nvs_set_str(const char *key, const char *value)
{
    if (key == NULL || value == NULL) return ESP_ERR_INVALID_ARG;
    if (!s_inited) return ESP_ERR_INVALID_STATE;

    esp_err_t err = nvs_set_str(s_handle, key, value);
    if (err != ESP_OK) return xlate(err);
    return nvs_commit(s_handle);
}

esp_err_t hal_nvs_get_str(const char *key, char *out, size_t out_size)
{
    if (key == NULL || out == NULL || out_size == 0) return ESP_ERR_INVALID_ARG;
    if (!s_inited) return ESP_ERR_INVALID_STATE;

    size_t len = out_size;
    return xlate(nvs_get_str(s_handle, key, out, &len));
}

esp_err_t hal_nvs_set_blob(const char *key, const void *data, size_t len)
{
    if (key == NULL || (data == NULL && len > 0)) return ESP_ERR_INVALID_ARG;
    if (!s_inited) return ESP_ERR_INVALID_STATE;

    esp_err_t err = nvs_set_blob(s_handle, key, data, len);
    if (err != ESP_OK) return xlate(err);
    return nvs_commit(s_handle);
}

esp_err_t hal_nvs_get_blob(const char *key, void *out, size_t out_size,
                           size_t *out_len)
{
    if (key == NULL || out == NULL) return ESP_ERR_INVALID_ARG;
    if (!s_inited) return ESP_ERR_INVALID_STATE;

    size_t len = out_size;
    esp_err_t err = nvs_get_blob(s_handle, key, out, &len);
    if (err != ESP_OK) return xlate(err);
    if (out_len != NULL) {
        *out_len = len;
    }
    return ESP_OK;
}

esp_err_t hal_nvs_erase(const char *key)
{
    if (key == NULL) return ESP_ERR_INVALID_ARG;
    if (!s_inited) return ESP_ERR_INVALID_STATE;

    esp_err_t err = nvs_erase_key(s_handle, key);
    if (err != ESP_OK) return xlate(err);
    return nvs_commit(s_handle);
}
