/**
 * @file hal_nvs.h
 * @brief Non-volatile key-value storage HAL (C6) — public interface.
 *
 * Logical key-value persistence for config, credentials, and small blobs.
 * Wraps ESP-IDF NVS on target; a file-backed mock provides the same API for
 * host unit tests. No @c nvs_flash.h leaks here, so the interface compiles with
 * plain gcc. All operations use a single fixed namespace owned by the HAL.
 *
 * @see docs/architecture/hal-design.md — HAL pattern + boundary rules.
 */
#ifndef HAL_NVS_H
#define HAL_NVS_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "platform_compat.h"   /* esp_err_t (target: esp_err.h; host: shim) */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the NVS backing store, with corruption recovery.
 *
 * Opens the HAL namespace read-write. If the NVS partition has no free pages or
 * a version mismatch (@c ESP_ERR_NVS_NO_FREE_PAGES / @c NEW_VERSION_FOUND), it
 * is erased and re-initialized to factory defaults (an empty store, write
 * counter reset to 0).
 *
 * @param[out] recovered Set to @c true if corruption recovery ran, @c false
 *                       otherwise. May be NULL. The caller can log/alarm on it
 *                       (the recovery event also surfaces as a BACnet alarm).
 * @return @c ESP_OK on success, or an @c esp_err_t from the backend.
 */
esp_err_t hal_nvs_init(bool *recovered);

/**
 * @brief Force any pending (coalesced) writes to be committed now.
 *
 * Writes are normally coalesced — committed ~2 s after the last write — to
 * absorb bursts (e.g. dragging a UI slider). Call this to commit immediately,
 * e.g. before reboot or on a deliberate save.
 *
 * @return @c ESP_OK on success, or @c ESP_ERR_INVALID_STATE if not initialized.
 */
esp_err_t hal_nvs_flush(void);

/**
 * @brief Total number of NVS commits since the counter was created.
 *
 * Persisted across reboots under the reserved key "__hal_nvs_writes"; reset to 0
 * by corruption recovery. Intended to map to BACnet Analog Input instance 303
 * for flash-wear observability.
 *
 * @return The commit count (0 if not initialized).
 */
uint32_t hal_nvs_get_write_count(void);

/**
 * @brief Store a NUL-terminated string under @p key.
 * @param key   Key name (non-NULL, namespace-unique).
 * @param value NUL-terminated value (non-NULL).
 * @retval ESP_OK              Stored and committed.
 * @retval ESP_ERR_INVALID_ARG @p key or @p value is NULL.
 * @retval ESP_ERR_INVALID_STATE Not initialized.
 */
esp_err_t hal_nvs_set_str(const char *key, const char *value);

/**
 * @brief Read a string value into @p out.
 * @param[in]  key      Key name (non-NULL).
 * @param[out] out      Destination buffer (non-NULL).
 * @param[in]  out_size Size of @p out in bytes (must be > 0).
 * @retval ESP_OK              Value copied (NUL-terminated).
 * @retval ESP_ERR_NOT_FOUND   No value stored under @p key.
 * @retval ESP_ERR_INVALID_ARG Bad argument.
 * @retval ESP_ERR_INVALID_STATE Not initialized.
 */
esp_err_t hal_nvs_get_str(const char *key, char *out, size_t out_size);

/**
 * @brief Store an opaque blob under @p key.
 * @param key  Key name (non-NULL).
 * @param data Blob bytes (non-NULL unless @p len is 0).
 * @param len  Number of bytes.
 * @retval ESP_OK / ESP_ERR_INVALID_ARG / ESP_ERR_INVALID_STATE
 */
esp_err_t hal_nvs_set_blob(const char *key, const void *data, size_t len);

/**
 * @brief Read a blob into @p out.
 * @param[in]  key      Key name (non-NULL).
 * @param[out] out      Destination buffer (non-NULL).
 * @param[in]  out_size Capacity of @p out in bytes.
 * @param[out] out_len  Receives the number of bytes read (may be NULL).
 * @retval ESP_OK / ESP_ERR_NOT_FOUND / ESP_ERR_INVALID_ARG / ESP_ERR_INVALID_STATE
 */
esp_err_t hal_nvs_get_blob(const char *key, void *out, size_t out_size,
                           size_t *out_len);

/**
 * @brief Erase the value stored under @p key.
 * @param key Key name (non-NULL).
 * @retval ESP_OK              Erased and committed.
 * @retval ESP_ERR_NOT_FOUND   No value stored under @p key.
 * @retval ESP_ERR_INVALID_ARG / ESP_ERR_INVALID_STATE
 */
esp_err_t hal_nvs_erase(const char *key);

#ifdef __cplusplus
}
#endif

#endif /* HAL_NVS_H */
