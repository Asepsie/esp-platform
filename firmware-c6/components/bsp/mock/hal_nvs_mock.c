/**
 * @file hal_nvs_mock.c
 * @brief Host mock of the NVS HAL — file-backed key-value store.
 *
 * Each key is a file under /tmp/nvs_sim/. Plain POSIX, no ESP-IDF, so it builds
 * with gcc and survives within a test process. Keys are used verbatim as file
 * names, so tests should use filename-safe keys (no '/').
 */
#include "hal_nvs_mock.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

#define HAL_NVS_DIR "/tmp/nvs_sim"

static bool s_inited;

static void key_path(const char *key, char *out, size_t out_size)
{
    snprintf(out, out_size, "%s/%s", HAL_NVS_DIR, key);
}

esp_err_t hal_nvs_init(void)
{
    if (mkdir(HAL_NVS_DIR, 0777) != 0 && errno != EEXIST) {
        return ESP_FAIL;
    }
    s_inited = true;
    return ESP_OK;
}

static esp_err_t write_file(const char *key, const void *data, size_t len)
{
    char path[256];
    key_path(key, path, sizeof(path));
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        return ESP_FAIL;
    }
    size_t wrote = (len > 0) ? fwrite(data, 1, len, f) : 0;
    fclose(f);
    return (wrote == len) ? ESP_OK : ESP_FAIL;
}

// Read a key file into buf (up to cap bytes). Returns bytes read via *out_n,
// or ESP_ERR_NOT_FOUND if the key file does not exist.
static esp_err_t read_file(const char *key, void *buf, size_t cap, size_t *out_n)
{
    char path[256];
    key_path(key, path, sizeof(path));
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return ESP_ERR_NOT_FOUND;
    }
    size_t n = fread(buf, 1, cap, f);
    fclose(f);
    if (out_n != NULL) {
        *out_n = n;
    }
    return ESP_OK;
}

esp_err_t hal_nvs_set_str(const char *key, const char *value)
{
    if (key == NULL || value == NULL) return ESP_ERR_INVALID_ARG;
    if (!s_inited) return ESP_ERR_INVALID_STATE;
    return write_file(key, value, strlen(value));
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
    return write_file(key, data, len);
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
    return ESP_OK;
}

void hal_nvs_mock_reset(void)
{
    DIR *d = opendir(HAL_NVS_DIR);
    if (d != NULL) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) {
                continue;
            }
            char path[256];
            key_path(e->d_name, path, sizeof(path));
            unlink(path);
        }
        closedir(d);
    }
    s_inited = false;
}
