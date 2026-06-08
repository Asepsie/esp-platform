# Real-Time Rules & Contracts
> **Thermostat Firmware — Architecture Document**
> Version: 1.0 | Status: Authoritative
> These rules are non-negotiable. Every module, every PR, every Claude Code session must comply.

---

## RT-01 — Task budget table

All FreeRTOS tasks are defined here. No task may be created outside this table without a PR updating it.

| Task | Priority | Stack | Max CPU/cycle | Deadline | Type |
|---|---|---|---|---|---|
| `sensor_state_writer` | 7 | 2 KB | 1 ms | Hard | ISR-adjacent |
| `zigbee_bridge_rx` | 6 | 8 KB | 20 ms | Soft | Event-driven |
| `control_loop` | 5 | 4 KB | 5 ms | Hard 1 s | Periodic |
| `bacnet_server` | 4 | 6 KB | 50 ms | Soft | Event-driven |
| `lvgl_ui` | 3 | 6 KB | 16 ms | Soft 60 fps | Periodic |
| `ota_manager` | 2 | 8 KB | Unlimited | None | Background |
| `rt_monitor` | 1 | 2 KB | 1 ms | None | Debug only |

Higher number = higher FreeRTOS priority.
`control_loop` is the only hard real-time task. It must never miss its 1 s deadline.
The Zigbee coordinator runs on the H2 coprocessor, **not** the C6. On the C6,
`zigbee_bridge_rx` receives H2 sensor reports over the UART bridge and is the only
writer of Layer 1 state.

---

## RT-02 — No blocking calls in high-priority tasks

Tasks at priority >= 4 must never block indefinitely.

```c
// FORBIDDEN in tasks priority >= 4
vTaskDelay(n);                        // use vTaskDelayUntil() for periodic tasks
recv(sock, buf, len, 0);              // always use select() with timeout first
xQueueReceive(q, &item, portMAX_DELAY); // always use a finite timeout

// REQUIRED pattern for control_loop (periodic hard-RT task)
void control_loop_task(void *arg) {
    TickType_t last_wake = xTaskGetTickCount();
    while (1) {
        esp_task_wdt_reset();
        // --- work here, must complete < 5 ms ---
        control_loop_tick();
        // ----------------------------------------
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CONTROL_LOOP_PERIOD_MS));
    }
}
```

---

## RT-03 — ISR contract

ISRs must be short, fast, and deterministic.

**ISRs MAY only:**
- Read/write a `volatile` flag or `atomic_uint`
- Call `xSemaphoreGiveFromISR()`
- Call `xQueueSendFromISR()` with a pre-allocated fixed-size item
- Call `xEventGroupSetBitsFromISR()`

**ISRs MAY NEVER:**
- Call any function not marked `IRAM_ATTR`
- Allocate memory
- Access NVS, flash, or any slow peripheral
- Call non-ISR-safe FreeRTOS APIs
- Hold any mutex

All ISR handler functions must be declared `IRAM_ATTR` and placed in `iram_safe` source sections.

---

## RT-04 — Shared state through store API only

The `thermostat_state_t` struct must never be accessed directly across task boundaries.

```c
// FORBIDDEN — direct cross-task struct access
extern thermostat_state_t g_state;
g_state.spaces[0].aggregated.avg_temperature = val; // data race

// REQUIRED — use the store API (mutex inside, max hold RT-05)
sensor_state_update_attribute(ieee, cluster, attr, value); // zigbee_bridge_rx (UART)
sensor_state_get_space(space_id, &space_out);              // control loop
sensor_state_set_recipe(&recipe);                          // BACnet task / UI
```

The store API is the only cross-task interface for state data. No exceptions.

---

## RT-05 — Mutex hold time budgets

Any mutex held longer than its budget is a bug, not a performance issue.

| Mutex | Owner | Max hold time | Violation class |
|---|---|---|---|
| `state.mutex` | sensor_state API | 1 ms | Hard bug |
| `bacnet.mutex` | BACnet object model | 5 ms | Hard bug |
| `lvgl.mutex` | LVGL port | 16 ms | Hard bug |
| `nvs.mutex` | NVS write operations | 50 ms | Soft warning |

**Debug enforcement** (enabled via `CONFIG_RT_DEBUG=y` in sdkconfig):

```c
// platform/target/rt_debug.c
esp_err_t rt_mutex_take_checked(SemaphoreHandle_t m, uint32_t budget_ms,
                                  const char *file, int line) {
    TickType_t start = xTaskGetTickCount();
    if (xSemaphoreTake(m, pdMS_TO_TICKS(budget_ms + 1)) != pdTRUE) {
        ESP_LOGE("RT", "Mutex timeout at %s:%d (budget %u ms)", file, line, budget_ms);
        return ESP_ERR_TIMEOUT;
    }
    // Hold time checked on release via paired macro
    return ESP_OK;
}

#ifdef CONFIG_RT_DEBUG
#define MUTEX_TAKE(m, budget_ms) \
    rt_mutex_take_checked(m, budget_ms, __FILE__, __LINE__)
#else
#define MUTEX_TAKE(m, budget_ms) \
    xSemaphoreTake(m, pdMS_TO_TICKS(budget_ms))
#endif
```

---

## RT-06 — No dynamic allocation after init

All memory is allocated once, statically, during `app_main()` before any task starts.

```c
// FORBIDDEN at runtime (after tasks started)
malloc()   free()   realloc()   pvPortMalloc()   pvPortFree()

// REQUIRED — static FreeRTOS allocation
static StaticTask_t     s_control_task_buf;
static StackType_t      s_control_stack[CONTROL_LOOP_STACK_SIZE];
static StaticQueue_t    s_zb_event_queue_buf;
static uint8_t          s_zb_event_storage[ZB_EVENT_QUEUE_DEPTH * sizeof(zb_event_t)];

xTaskCreateStatic(control_loop_task, "control", CONTROL_LOOP_STACK_SIZE,
                  NULL, CONTROL_LOOP_PRIORITY,
                  s_control_stack, &s_control_task_buf);
```

All queue depths, buffer sizes, and max device counts are compile-time constants
defined in `config/thermostat_config.h`.

---

## RT-07 — Watchdog registration mandatory

Every task must register with the task watchdog and reset it within its deadline.

```c
void any_task(void *arg) {
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL)); // register this task
    while (1) {
        esp_task_wdt_reset();                // must call within WDT timeout
        // ... task work ...
    }
}
```

WDT timeout is configured to 2× the longest task period (control loop = 1 s → WDT = 2 s).
A deadlocked task triggers a system reset and generates a backtrace log to NVS.

---

## RT-08 — Abstract timing — no hardware timing assumptions

All timing must go through the platform abstraction layer. This rule is what makes QEMU and host unit tests work without modification.

```c
// FORBIDDEN — hardware-specific or assumes tick rate
ets_delay_us(100);
vTaskDelay(1);           // 1 tick ≠ 1 ms on all configs

// REQUIRED — platform-abstracted
#include "platform/platform_time.h"
platform_delay_ms(1);    // → vTaskDelay on target, usleep on host
platform_get_ms();       // → esp_timer_get_time()/1000 on target, clock() on host
```

---

## RT-09 — Deadline miss monitoring

The control loop tracks its own deadline misses. Misses are observable via BACnet and LVGL.

```c
// In control_loop.c
static atomic_uint s_deadline_miss_count = 0;

void control_loop_task(void *arg) {
    TickType_t last_wake = xTaskGetTickCount();
    while (1) {
        esp_task_wdt_reset();
        TickType_t now = xTaskGetTickCount();
        if ((now - last_wake) > pdMS_TO_TICKS(CONTROL_LOOP_PERIOD_MS + 10)) {
            atomic_fetch_add(&s_deadline_miss_count, 1);
        }
        control_loop_tick();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CONTROL_LOOP_PERIOD_MS));
    }
}

uint32_t control_loop_get_deadline_misses(void) {
    return atomic_load(&s_deadline_miss_count);
}
```

This value is exposed as BACnet Analog Input instance 300 (`Diag-DeadlineMisses`).

---

## Stack high-water mark monitoring (debug builds only)

```c
// rt_monitor_task — priority 1, runs every 30 s in CONFIG_RT_DEBUG builds
void rt_monitor_task(void *arg) {
    while (1) {
        ESP_LOGI("RT_MON", "Stack HWM — control:%u bridge:%u bacnet:%u ui:%u",
            uxTaskGetStackHighWaterMark(h_control_task),
            uxTaskGetStackHighWaterMark(h_zigbee_bridge_task),
            uxTaskGetStackHighWaterMark(h_bacnet_task),
            uxTaskGetStackHighWaterMark(h_ui_task));
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}
```

If any task's HWM drops below 256 bytes, the stack size in RT-01 must be increased before release.
