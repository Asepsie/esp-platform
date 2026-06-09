/**
 * @file hal_nvs_mock.c
 * @brief Host mock of the NVS HAL — file-backed, with testable coalescing.
 *
 * Each key is a file under /tmp/nvs_sim/. Mirrors the target's observable
 * behaviour:
 *   - Writes update the backing files immediately (so reads see them) but only
 *     mark state dirty; a "commit" is deferred until 2 s of write inactivity on
 *     the SIMULATED clock (advanced by hal_nvs_mock_advance_ms()) or a flush.
 *   - The write counter increments per commit and is persisted in the reserved
 *     "__hal_nvs_writes" file.
 *   - hal_nvs_mock_set_corrupt() makes the next init perform factory-reset
 *     recovery.
 *
 * Note: unlike real NVS, data is durable before commit here — fidelity is
 * intentionally limited to commit *coalescing* (the counter), which is what the
 * tests exercise. Plain POSIX, no ESP-IDF. Keys must be filename-safe.
 */
#include "hal_nvs_mock.h"
#include "thermostat_config.h"  // HAL_NVS_COALESCE_MS

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

#define HAL_NVS_DIR          "/tmp/nvs_sim"
#define HAL_NVS_COUNT_KEY    "__hal_nvs_writes"

static bool     s_inited;
static bool     s_dirty;
static uint32_t s_sim_now_ms;
static uint32_t s_last_write_ms;
static uint32_t s_write_count;
static bool     s_corrupt_next;

static void key_path(const char *key, char *out, size_t out_size)
{
    snprintf(out, out_size, "%s/%s", HAL_NVS_DIR, key);
}

static esp_err_t write_file(const char *key, const void *data, size_t len)
{
    char path[256];
    key_path(key, path, sizeof(path));
    FILE *f = fopen(path, "wb");
    if (f == NULL) return ESP_FAIL;
    size_t wrote = (len > 0) ? fwrite(data, 1, len, f) : 0;
    fclose(f);
    return (wrote == len) ? ESP_OK : ESP_FAIL;
}

static esp_err_t read_file(const char *key, void *buf, size_t cap, size_t *out_n)
{
    char path[256];
    key_path(key, path, sizeof(path));
    FILE *f = fopen(path, "rb");
    if (f == NULL) return ESP_ERR_NOT_FOUND;
    size_t n = fread(buf, 1, cap, f);
    fclose(f);
    if (out_n != NULL) *out_n = n;
    return ESP_OK;
}

static void wipe_dir(void)
{
    DIR *d = opendir(HAL_NVS_DIR);
    if (d == NULL) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char path[256];
        key_path(e->d_name, path, sizeof(path));
        unlink(path);
    }
    closedir(d);
}

static void save_counter(void)
{
    write_file(HAL_NVS_COUNT_KEY, &s_write_count, sizeof(s_write_count));
}

static void load_counter(void)
{
    size_t n = 0;
    uint32_t v = 0;
    if (read_file(HAL_NVS_COUNT_KEY, &v, sizeof(v), &n) == ESP_OK && n == sizeof(v)) {
        s_write_count = v;
    } else {
        s_write_count = 0;
    }
}

// One coalesced commit: persists the bumped counter, clears dirty.
static void do_commit(void)
{
    if (!s_dirty) return;
    s_write_count++;
    save_counter();
    s_dirty = false;
}

static void mark_dirty(void)
{
    s_dirty = true;
    s_last_write_ms = s_sim_now_ms;
}

esp_err_t hal_nvs_init(bool *recovered)
{
    if (mkdir(HAL_NVS_DIR, 0777) != 0 && errno != EEXIST) {
        return ESP_FAIL;
    }
    if (s_corrupt_next) {
        wipe_dir();              // factory reset
        s_write_count = 0;
        s_corrupt_next = false;
        if (recovered) *recovered = true;
    } else {
        load_counter();
        if (recovered) *recovered = false;
    }
    s_dirty = false;
    s_sim_now_ms = 0;
    s_last_write_ms = 0;
    s_inited = true;
    return ESP_OK;
}

esp_err_t hal_nvs_set_str(const char *key, const char *value)
{
    if (key == NULL || value == NULL) return ESP_ERR_INVALID_ARG;
    if (!s_inited) return ESP_ERR_INVALID_STATE;
    esp_err_t err = write_file(key, value, strlen(value));
    if (err == ESP_OK) mark_dirty();
    return err;
}

esp_err_t hal_nvs_get_str(const char *key, char *out, size_t out_size)
{
    if (key == NULL || out == NULL || out_size == 0) return ESP_ERR_INVALID_ARG;
    if (!s_inited) return ESP_ERR_INVALID_STATE;
    size_t n = 0;
    esp_err_t err = read_file(key, out, out_size - 1, &n);
    if (err != ESP_OK) return err;
    out[n] = '\0';
    return ESP_OK;
}

esp_err_t hal_nvs_set_blob(const char *key, const void *data, size_t len)
{
    if (key == NULL || (data == NULL && len > 0)) return ESP_ERR_INVALID_ARG;
    if (!s_inited) return ESP_ERR_INVALID_STATE;
    esp_err_t err = write_file(key, data, len);
    if (err == ESP_OK) mark_dirty();
    return err;
}

esp_err_t hal_nvs_get_blob(const char *key, void *out, size_t out_size,
                           size_t *out_len)
{
    if (key == NULL || out == NULL) return ESP_ERR_INVALID_ARG;
    if (!s_inited) return ESP_ERR_INVALID_STATE;
    return read_file(key, out, out_size, out_len);
}

esp_err_t hal_nvs_erase(const char *key)
{
    if (key == NULL) return ESP_ERR_INVALID_ARG;
    if (!s_inited) return ESP_ERR_INVALID_STATE;
    char path[256];
    key_path(key, path, sizeof(path));
    if (unlink(path) != 0) {
        return (errno == ENOENT) ? ESP_ERR_NOT_FOUND : ESP_FAIL;
    }
    mark_dirty();
    return ESP_OK;
}

esp_err_t hal_nvs_flush(void)
{
    if (!s_inited) return ESP_ERR_INVALID_STATE;
    do_commit();
    return ESP_OK;
}

uint32_t hal_nvs_get_write_count(void)
{
    return s_inited ? s_write_count : 0;
}

// --- test-only helpers --------------------------------------------------------

void hal_nvs_mock_reset(void)
{
    wipe_dir();
    s_inited = false;
    s_dirty = false;
    s_sim_now_ms = 0;
    s_last_write_ms = 0;
    s_write_count = 0;
    s_corrupt_next = false;
}

void hal_nvs_mock_advance_ms(uint32_t ms)
{
    s_sim_now_ms += ms;
    if (s_dirty && (s_sim_now_ms - s_last_write_ms) >= HAL_NVS_COALESCE_MS) {
        do_commit();
    }
}

void hal_nvs_mock_set_corrupt(bool corrupt)
{
    s_corrupt_next = corrupt;
}
