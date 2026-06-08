// =============================================================================
// platform_compat.h — thin target/host portability shim.
//
// Lets logic components (sensor_state, control, ...) compile both on ESP-IDF
// targets and in plain host unit tests. It provides three things:
//   * esp_err_t + ESP_OK/ESP_ERR_* (real esp_err.h on target; stubs on host)
//   * platform_mutex_*  (FreeRTOS mutex on target; no-op on host)
//   * platform_now_ms() (esp_timer on target; CLOCK_MONOTONIC on host)
//
// ESP_PLATFORM is defined by the ESP-IDF build system; absence => host build.
//
// NOTE: the host mutex is a no-op. Host unit tests are single-threaded, so this
// is sufficient to exercise the store's logic. It does NOT validate locking;
// concurrency is covered on target / in integration tests.
// =============================================================================
#ifndef PLATFORM_COMPAT_H
#define PLATFORM_COMPAT_H

#include <stdint.h>
#include <stdbool.h>

#if defined(ESP_PLATFORM)
// ---- Target (ESP-IDF) -------------------------------------------------------
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef SemaphoreHandle_t platform_mutex_t;

static inline platform_mutex_t platform_mutex_create(void) {
    return xSemaphoreCreateMutex();
}
static inline bool platform_mutex_lock(platform_mutex_t m) {
    return xSemaphoreTake(m, portMAX_DELAY) == pdTRUE;
}
static inline void platform_mutex_unlock(platform_mutex_t m) {
    xSemaphoreGive(m);
}
static inline uint32_t platform_now_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

#else
// ---- Host (native gcc, unit tests) ------------------------------------------
#include <time.h>

// esp_err_t and the subset of codes the logic components use. Values mirror
// ESP-IDF's esp_err.h so assertions behave identically on host and target.
typedef int esp_err_t;
#define ESP_OK                  0
#define ESP_FAIL                -1
#define ESP_ERR_NO_MEM          0x101
#define ESP_ERR_INVALID_ARG     0x102
#define ESP_ERR_INVALID_STATE   0x103
#define ESP_ERR_NOT_FOUND       0x105
#define ESP_ERR_NOT_SUPPORTED   0x106

typedef void *platform_mutex_t;

static inline platform_mutex_t platform_mutex_create(void) {
    static int dummy;       // any stable non-NULL handle
    return &dummy;
}
static inline bool platform_mutex_lock(platform_mutex_t m) {
    (void)m;
    return true;
}
static inline void platform_mutex_unlock(platform_mutex_t m) {
    (void)m;
}
static inline uint32_t platform_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u);
}

#endif // ESP_PLATFORM

#endif // PLATFORM_COMPAT_H
