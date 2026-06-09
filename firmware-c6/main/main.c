// firmware-c6 — ESP32-C6 primary MCU entry point.
// Scaffold: brings up the FreeRTOS app and prints a liveness banner.
// Real work (BACnet, control loop, UI, OTA, UART bridge) is added in later modules.

#include "esp_log.h"
#include "esp_err.h"
#include "hal_gpio.h"
#include "control_loop.h"
#include "control_task.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "C6 alive");

    // Bring up GPIO (relays/LED safe-low, H2 held in reset), then release H2.
    ESP_ERROR_CHECK(hal_gpio_init());
    ESP_ERROR_CHECK(hal_gpio_set(HAL_GPIO_H2_EN, true));

    // Control loop for the single zone (relays start off), then start the
    // 1 Hz control task (RT-01) that drives control_loop_tick().
    ESP_ERROR_CHECK(control_loop_init("zone_a"));
    ESP_ERROR_CHECK(control_task_start());

    // app_main returns — FreeRTOS scheduler keeps running launched tasks.
}
