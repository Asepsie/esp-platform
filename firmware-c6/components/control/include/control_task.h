/**
 * @file control_task.h
 * @brief FreeRTOS task that runs the control loop at 1 Hz (RT-01).
 *
 * Thin runtime wrapper around @c control_loop_tick(). Target-only (FreeRTOS +
 * task watchdog) — host unit tests exercise the loop logic via control_loop.h
 * directly, not this task.
 *
 * @see docs/architecture/rt-rules-v2.md — RT-01 (budget), RT-02 (no blocking),
 *      RT-06 (static alloc), RT-07 (watchdog), RT-09 (deadline misses).
 */
#ifndef CONTROL_TASK_H
#define CONTROL_TASK_H

#include "platform_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create and start the control task.
 *
 * Statically allocated (RT-06), priority 5, 4 KB stack, 1 s period. Must be
 * called once during startup, after @c hal_gpio_init() and
 * @c control_loop_init(). Idempotent: a second call is a no-op.
 *
 * @return @c ESP_OK on success, @c ESP_ERR_NO_MEM if task creation failed,
 *         @c ESP_ERR_INVALID_STATE if already started.
 */
esp_err_t control_task_start(void);

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_TASK_H */
