/**
 * @file hal_nvs.c
 * @brief NVS storage HAL — ESP32-C6 target implementation.
 *
 * Beyond plain NVS wrapping this layer adds:
 *   1. Write coalescing — set/erase update the NVS cache immediately (so reads
 *      see them) but defer @c nvs_commit() until 2 s of write inactivity, or an
 *      explicit @c hal_nvs_flush(). Absorbs burst writes (e.g. UI slider drags).
 *      Implemented with a one-shot esp_timer rearmed on every write.
 *   2. Write counter — total commits, persisted under "__hal_nvs_writes",
 *      exposed via @c hal_nvs_get_write_count() (→ BACnet AI 303).
 *   3. Corruption recovery — @c hal_nvs_init() erases + reinitializes on a
 *      no-free-pages / version mismatch and reports it via @p recovered.
 *
 * Missing keys are normalized to @c ESP_ERR_NOT_FOUND. A mutex guards state
 * shared between caller tasks and the commit-timer callback.
 */
#include "hal_nvs.h"

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "thermostat_config.h"  // HAL_NVS_COALESCE_MS

#define HAL_NVS_NAMESPACE        "hal_kv"
#define HAL_NVS_WRITE_COUNT_KEY  "__hal_nvs_writes"
#define HAL_NVS_COALESCE_US      (HAL_NVS_COALESCE_MS * 1000)  // commit coalescing window

static nvs_handle_t       s_handle;
static bool               s_inited;
static bool               s_dirty;        // uncommitted changes pending
static uint32_t           s_write_count;  // total commits (persisted)
static SemaphoreHandle_t  s_mutex;
static esp_timer_handle_t s_commit_timer;

// Normalize backend "not found" to the portable code used across the HAL.
static esp_err_t xlate(esp_err_t err)
{
    return (err == ESP_ERR_NVS_NOT_FOUND) ? ESP_ERR_NOT_FOUND : err;
}

// Commit pending changes (and the bumped write counter) as one NVS commit.
// Caller must hold s_mutex.
static void do_commit_locked(void)
{
    if (!s_dirty) {
        return;
    }
    s_write_count++;
    nvs_set_u32(s_handle, HAL_NVS_WRITE_COUNT_KEY, s_write_count);
    nvs_commit(s_handle);
    s_dirty = false;
}

static void commit_timer_cb(void *arg)
{
    (void)arg;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    do_commit_locked();
    xSemaphoreGive(s_mutex);
}

// Mark state dirty and (re)arm the coalescing timer. Caller holds s_mutex.
static void mark_dirty_locked(void)
{
    s_dirty = true;
    esp_timer_stop(s_commit_timer); // no-op/ignored if not currently running
    esp_timer_start_once(s_commit_timer, HAL_NVS_COALESCE_US);
}

esp_err_t hal_nvs_init(bool *recovered)
{
    bool rec = false;

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        rec = true; // corruption / version change → factory reset of NVS
        err = nvs_flash_erase();
        if (err == ESP_OK) {
            err = nvs_flash_init();
        }
    }
    if (err == ESP_OK) {
        err = nvs_open(HAL_NVS_NAMESPACE, NVS_READWRITE, &s_handle);
    }
    if (err != ESP_OK) {
        if (recovered) *recovered = rec;
        return err;
    }

    // Load the persisted commit counter (absent after a recovery erase → 0).
    uint32_t cnt = 0;
    if (nvs_get_u32(s_handle, HAL_NVS_WRITE_COUNT_KEY, &cnt) != ESP_OK) {
        cnt = 0;
    }
    s_write_count = cnt;

    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
        if (s_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    if (s_commit_timer == NULL) {
        const esp_timer_create_args_t targs = {
            .callback = commit_timer_cb,
            .name     = "hal_nvs_commit",
        };
        err = esp_timer_create(&targs, &s_commit_timer);
        if (err != ESP_OK) {
            return err;
        }
    }

    s_dirty = false;
    s_inited = true;
    if (recovered) *recovered = rec;
    return ESP_OK;
}

esp_err_t hal_nvs_set_str(const char *key, const char *value)
{
    if (key == NULL || value == NULL) return ESP_ERR_INVALID_ARG;
    if (!s_inited) return ESP_ERR_INVALID_STATE;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    esp_err_t err = nvs_set_str(s_handle, key, value);
    if (err == ESP_OK) {
        mark_dirty_locked();
    }
    xSemaphoreGive(s_mutex);
    return xlate(err);
}

esp_err_t hal_nvs_get_str(const char *key, char *out, size_t out_size)
{
    if (key == NULL || out == NULL || out_size == 0) return ESP_ERR_INVALID_ARG;
    if (!s_inited) return ESP_ERR_INVALID_STATE;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    size_t len = out_size;
    esp_err_t err = nvs_get_str(s_handle, key, out, &len);
    xSemaphoreGive(s_mutex);
    return xlate(err);
}

esp_err_t hal_nvs_set_blob(const char *key, const void *data, size_t len)
{
    if (key == NULL || (data == NULL && len > 0)) return ESP_ERR_INVALID_ARG;
    if (!s_inited) return ESP_ERR_INVALID_STATE;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    esp_err_t err = nvs_set_blob(s_handle, key, data, len);
    if (err == ESP_OK) {
        mark_dirty_locked();
    }
    xSemaphoreGive(s_mutex);
    return xlate(err);
}

esp_err_t hal_nvs_get_blob(const char *key, void *out, size_t out_size,
                           size_t *out_len)
{
    if (key == NULL || out == NULL) return ESP_ERR_INVALID_ARG;
    if (!s_inited) return ESP_ERR_INVALID_STATE;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    size_t len = out_size;
    esp_err_t err = nvs_get_blob(s_handle, key, out, &len);
    xSemaphoreGive(s_mutex);
    if (err != ESP_OK) {
        return xlate(err);
    }
    if (out_len != NULL) {
        *out_len = len;
    }
    return ESP_OK;
}

esp_err_t hal_nvs_erase(const char *key)
{
    if (key == NULL) return ESP_ERR_INVALID_ARG;
    if (!s_inited) return ESP_ERR_INVALID_STATE;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    esp_err_t err = nvs_erase_key(s_handle, key);
    if (err == ESP_OK) {
        mark_dirty_locked();
    }
    xSemaphoreGive(s_mutex);
    return xlate(err);
}

esp_err_t hal_nvs_flush(void)
{
    if (!s_inited) return ESP_ERR_INVALID_STATE;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    esp_timer_stop(s_commit_timer);
    do_commit_locked();
    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

uint32_t hal_nvs_get_write_count(void)
{
    if (!s_inited) return 0;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    uint32_t v = s_write_count;
    xSemaphoreGive(s_mutex);
    return v;
}
