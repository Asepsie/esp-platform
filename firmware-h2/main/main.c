// firmware-h2 — ESP32-H2 Zigbee coprocessor entry point.
// Brings up the UART bridge to the C6. The Zigbee coordinator (which will feed
// sensor reports into the bridge) is added in a later module.

#include "esp_log.h"
#include "uart_bridge.h"

static const char *TAG = "main";

// Called from the bridge RX task for each decoded C6 -> H2 command frame.
// For now we just log it; real handlers (permit-join, poll, OTA) come with the
// Zigbee coordinator.
static void on_bridge_cmd(bridge_msg_type_t type, const uint8_t *payload, uint16_t len)
{
    (void)payload;
    ESP_LOGI(TAG, "bridge cmd 0x%02X (%u bytes)", (unsigned)type, (unsigned)len);
}

void app_main(void)
{
    ESP_LOGI(TAG, "H2 alive");
    ESP_ERROR_CHECK(uart_bridge_init(on_bridge_cmd));
    // app_main returns — the bridge RX/heartbeat tasks keep running.
}
