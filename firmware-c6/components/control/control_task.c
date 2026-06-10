/**
 * @file control_task.c
 * @brief 1 Hz control FreeRTOS task (RT-01). Target-only.
 *
 * Periodic hard-real-time task that calls @c control_loop_tick() once per
 * second. Implements the RT rules for the control loop:
 *   RT-01  priority 5, 4 KB stack, periodic, hard 1 s deadline
 *   RT-02  no indefinite blocking — uses vTaskDelayUntil() for periodicity
 *   RT-06  static allocation (xTaskCreateStatic) — no heap after init
 *   RT-07  watchdog via hal_wdt (configure timeout, register, reset each cycle)
 *   RT-08  time read via hal_timer (no direct esp_timer/tick reads for timing)
 *   RT-09  detects its own deadline misses → sensor_state (BACnet AI 300)
 *
 * The periodic scheduling primitive (vTaskDelayUntil) stays direct — per the
 * RT-08 clarification, a target-only task wrapper may use it; the testable
 * timing/watchdog logic goes through the HAL.
 */
#include "control_task.h"
#include "control_loop.h"
#include "sensor_state.h"
#include "hal_timer.h"
#include "hal_wdt.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "thermostat_config.h"  // RT-01 budget: CONTROL_LOOP_PERIOD_MS/_PRIORITY/_STACK_SIZE

// Tolerance before a late wake-up is counted as a deadline miss (RT-09).
#define CONTROL_DEADLINE_SLACK_MS 10

static const char *TAG = "control";

// RT-06: task control block + stack allocated statically, once.
static StaticTask_t s_task_tcb;
static StackType_t  s_task_stack[CONTROL_LOOP_STACK_SIZE];
static TaskHandle_t s_task_handle;

static void control_task(void *arg)
{
    (void)arg;

    // RT-07: subscribe this task to the task watchdog.
    ESP_ERROR_CHECK(hal_wdt_add_current_task());

    TickType_t last_wake = xTaskGetTickCount();   // vTaskDelayUntil reference
    uint32_t   last_ms   = hal_timer_get_ms();    // deadline-miss measurement
    for (;;) {
        hal_wdt_reset(); // RT-07: pet the dog within the deadline

        // RT-09: if the gap since the previous cycle exceeded one period
        // (+slack), the scheduler delayed us past the 1 s deadline.
        const uint32_t now_ms = hal_timer_get_ms();
        if ((uint32_t)(now_ms - last_ms) >
            (CONTROL_LOOP_PERIOD_MS + CONTROL_DEADLINE_SLACK_MS)) {
            sensor_state_increment_deadline_miss();
        }
        last_ms = now_ms;

        control_loop_tick();

        // RT-02: bounded, periodic wait — never blocks indefinitely.
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CONTROL_LOOP_PERIOD_MS));
    }
}

esp_err_t control_task_start(void)
{
    if (s_task_handle != NULL) {
        return ESP_ERR_INVALID_STATE; // already started
    }
    // The watchdog timeout is configured once by app_main (hal_wdt_init);
    // the task itself subscribes via hal_wdt_add_current_task() at entry.
    s_task_handle = xTaskCreateStatic(control_task, "control",
                                      CONTROL_LOOP_STACK_SIZE, NULL,
                                      CONTROL_LOOP_PRIORITY,
                                      s_task_stack, &s_task_tcb);
    if (s_task_handle == NULL) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "control task started: prio %d, %d ms period",
             CONTROL_LOOP_PRIORITY, CONTROL_LOOP_PERIOD_MS);
    return ESP_OK;
}
