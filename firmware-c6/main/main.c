// firmware-c6 — ESP32-C6 primary MCU entry point.
// Brings up persistent storage, the central state store, GPIO, and the control
// loop. Later modules (BACnet, UI, OTA, zigbee_bridge) hook in from here.

#include "esp_log.h"
#include "esp_err.h"
#include "hal_gpio.h"
#include "hal_nvs.h"
#include "sensor_state.h"
#include "zigbee_bridge.h"
#include "control_loop.h"
#include "control_task.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "C6 alive");

    // 1. Persistent storage FIRST — device/topology/recipe config will be
    //    loaded from NVS by later modules. hal_nvs_init() self-heals a corrupt
    //    partition (erase + factory reset) and reports it via `recovered`.
    bool nvs_recovered = false;
    ESP_ERROR_CHECK(hal_nvs_init(&nvs_recovered));
    if (nvs_recovered) {
        ESP_LOGW(TAG, "NVS was corrupt — recovered to factory defaults");
    }

    // 2. Central state store — the only legal cross-task interface (RT-04).
    //    Must be up before the control task (which reads it) starts. Record NVS
    //    health as diagnostics: commit count → BACnet AI 303 (flash-wear), and
    //    the recovery flag so the BMS can observe/alarm a factory reset.
    ESP_ERROR_CHECK(sensor_state_init());
    sensor_state_set_nvs_status(nvs_recovered, hal_nvs_get_write_count());

    // 3. GPIO: relays/LED driven safe-low, H2 held in reset; then release H2.
    ESP_ERROR_CHECK(hal_gpio_init());
    ESP_ERROR_CHECK(hal_gpio_set(HAL_GPIO_H2_EN, true));

    // 4. UART bridge to the H2: starts the RX task that feeds Zigbee sensor
    //    reports / joins into the state store.
    ESP_ERROR_CHECK(zigbee_bridge_init());

    // 5. Control loop for the single zone (relays start off), then the 1 Hz
    //    control task (RT-01) that drives control_loop_tick().
    ESP_ERROR_CHECK(control_loop_init("zone_a"));
    ESP_ERROR_CHECK(control_task_start());

    // app_main returns — FreeRTOS scheduler keeps running launched tasks.
}
