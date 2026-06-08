// firmware-c6 — ESP32-C6 primary MCU entry point.
// Scaffold: brings up the FreeRTOS app and prints a liveness banner.
// Real work (BACnet, control loop, UI, OTA, UART bridge) is added in later modules.

#include "esp_log.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "C6 alive");
    // app_main returns — FreeRTOS scheduler keeps running launched tasks.
}
