// firmware-c6 — ESP32-C6 primary MCU entry point.
// Wires the subsystems together in a fixed init sequence. Every init is checked;
// on any failure the app logs the error, flashes the status LED in a fault
// pattern, and halts (does NOT proceed with a half-initialized system).

#include "esp_log.h"
#include "esp_err.h"
#include "hal_gpio.h"
#include "hal_nvs.h"
#include "hal_timer.h"
#include "hal_wdt.h"
#include "sensor_state.h"
#include "zigbee_bridge.h"
#include "control_loop.h"
#include "control_task.h"
#include "thermostat_config.h"   // TASK_WDT_TIMEOUT_S

static const char *TAG = "main";

// Halt-and-indicate: log the failing stage and blink the status LED forever.
// Best-effort — if GPIO init itself failed the LED writes are harmless no-ops,
// but the halt still holds so the app never runs half-initialized.
static void enter_fault(const char *stage, esp_err_t err)
{
    ESP_LOGE(TAG, "init failed at %s: %s — halting", stage, esp_err_to_name(err));
    for (;;) {
        (void)hal_gpio_set(HAL_GPIO_STATUS_LED, true);
        hal_timer_delay_ms(150);
        (void)hal_gpio_set(HAL_GPIO_STATUS_LED, false);
        hal_timer_delay_ms(150);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "C6 alive");
    esp_err_t err;

    // 1. Persistent storage first — later modules load config from it.
    //    hal_nvs_init() self-heals a corrupt partition and reports recovery.
    bool nvs_recovered = false;
    err = hal_nvs_init(&nvs_recovered);
    if (err != ESP_OK) {
        enter_fault("hal_nvs_init", err);
    }
    if (nvs_recovered) {
        ESP_LOGW(TAG, "NVS was corrupt — recovered to factory defaults");
    }

    // 2. GPIO: relays/LED driven safe-low, H2 held in reset; then release H2.
    err = hal_gpio_init();
    if (err != ESP_OK) {
        enter_fault("hal_gpio_init", err);
    }
    err = hal_gpio_set(HAL_GPIO_H2_EN, true); // release the H2 from reset
    if (err != ESP_OK) {
        enter_fault("hal_gpio_set(H2_EN)", err);
    }

    // 3. Timing HAL needs no init (esp_timer is brought up at boot). Listed for
    //    sequence completeness.

    // 4. Task watchdog timeout — set before any task subscribes.
    err = hal_wdt_init(TASK_WDT_TIMEOUT_S);
    if (err != ESP_OK) {
        enter_fault("hal_wdt_init", err);
    }

    // 5. Central state store (the only cross-task interface, RT-04). Record NVS
    //    health: commit count → BACnet AI 303, recovery flag for the BMS.
    err = sensor_state_init();
    if (err != ESP_OK) {
        enter_fault("sensor_state_init", err);
    }
    sensor_state_set_nvs_status(nvs_recovered, hal_nvs_get_write_count());

    // 6. UART bridge to the H2: starts the RX task that feeds Zigbee sensor
    //    reports / joins into the store.
    err = zigbee_bridge_init();
    if (err != ESP_OK) {
        enter_fault("zigbee_bridge_init", err);
    }

    // 7. Control loop for the single zone (relays start off).
    err = control_loop_init("zone_a");
    if (err != ESP_OK) {
        enter_fault("control_loop_init", err);
    }

    // 8. Start the 1 Hz control task (RT-01) that drives control_loop_tick().
    err = control_task_start();
    if (err != ESP_OK) {
        enter_fault("control_task_start", err);
    }

    ESP_LOGI(TAG, "init complete");
    // app_main returns — FreeRTOS scheduler keeps running launched tasks.
}
