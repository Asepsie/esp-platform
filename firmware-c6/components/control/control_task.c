/**
 * @file control_task.c
 * @brief 1 Hz control FreeRTOS task (RT-01). Target-only.
 *
 * Periodic hard-real-time task that calls @c control_loop_tick() once per
 * second. Implements the RT rules for the control loop:
 *   RT-01  priority 5, 4 KB stack, periodic, hard 1 s deadline
 *   RT-02  no indefinite blocking — uses vTaskDelayUntil() for periodicity
 *   RT-06  static allocation (xTaskCreateStatic) — no heap after init
 *   RT-07  registers with the task watchdog and resets it every cycle
 *   RT-09  detects its own deadline misses → sensor_state (BACnet AI 300)
 */
#include "control_task.h"
#include "control_loop.h"
#include "sensor_state.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_log.h"

// RT-01 budget. These will move to config/thermostat_config.h when it lands;
// values match the data-model config block (CONTROL_LOOP_PERIOD_MS / _PRIORITY).
#define CONTROL_LOOP_PERIOD_MS   1000
#define CONTROL_TASK_PRIORITY    5
#define CONTROL_TASK_STACK_SIZE  4096   // bytes (ESP-IDF StackType_t = uint8_t)

// Tolerance before a late wake-up is counted as a deadline miss (RT-09).
#define CONTROL_DEADLINE_SLACK_MS 10

static const char *TAG = "control";

// RT-06: task control block + stack allocated statically, once.
static StaticTask_t s_task_tcb;
static StackType_t  s_task_stack[CONTROL_TASK_STACK_SIZE];
static TaskHandle_t s_task_handle;

static void control_task(void *arg)
{
    (void)arg;

    // RT-07: subscribe this task to the task watchdog.
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        esp_task_wdt_reset(); // RT-07: pet the dog within the deadline

        // RT-09: if we woke more than one period (+slack) after the last
        // scheduled wake, the previous cycle missed its 1 s deadline.
        const TickType_t now = xTaskGetTickCount();
        if ((now - last_wake) >
            pdMS_TO_TICKS(CONTROL_LOOP_PERIOD_MS + CONTROL_DEADLINE_SLACK_MS)) {
            sensor_state_increment_deadline_miss();
        }

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
    s_task_handle = xTaskCreateStatic(control_task, "control",
                                      CONTROL_TASK_STACK_SIZE, NULL,
                                      CONTROL_TASK_PRIORITY,
                                      s_task_stack, &s_task_tcb);
    if (s_task_handle == NULL) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "control task started: prio %d, %d ms period",
             CONTROL_TASK_PRIORITY, CONTROL_LOOP_PERIOD_MS);
    return ESP_OK;
}
