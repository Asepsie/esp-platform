// firmware-h2 — ESP32-H2 Zigbee coprocessor entry point.
//
// Boots the UART bridge to the C6 and the Zigbee coordinator, wiring sensor
// reports and device-join events from the coordinator out to the C6, and
// dispatching commands received from the C6 into the coordinator.

#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"

#include "uart_bridge.h"
#include "uart_bridge_protocol.h"
#include "zigbee_coordinator.h"

static const char *TAG = "main";

// --- Coordinator -> C6 (outbound) --------------------------------------------
// The coordinator invokes these from its stack task; forward straight onto the
// UART bridge.
static void on_sensor_report(const bridge_sensor_report_t *r)
{
    uart_bridge_send_sensor_report(r);
}

static void on_device_join(const bridge_device_join_t *d)
{
    uart_bridge_send_device_join(d);
}

// --- C6 -> coordinator (inbound commands) ------------------------------------
// Decoded C6->H2 command frames arrive here (registered as the uart_bridge
// command callback). Dispatch is done in main — the composition root that knows
// both the bridge and the coordinator — rather than inside uart_bridge.c, which
// stays a pure transport that only delivers commands via this callback
// (uart-bridge-protocol.md §8). NOTE: the original task spec placed this in
// uart_bridge.c; doing it here avoids inverting the transport->app dependency.
static void on_bridge_cmd(bridge_msg_type_t type, const uint8_t *payload, uint16_t len)
{
    switch (type) {
        case MSG_PERMIT_JOIN:                       // duration_s[1]
            if (len >= 1) {
                zigbee_coordinator_permit_join(payload[0]);
            }
            break;

        case MSG_POLL_ATTR: {                       // ieee[8] + cluster[2] + attr[2]
            if (len >= 12) {
                uint16_t cluster = (uint16_t)payload[8]  | ((uint16_t)payload[9]  << 8);
                uint16_t attr    = (uint16_t)payload[10] | ((uint16_t)payload[11] << 8);
                zigbee_coordinator_poll_attr(payload, cluster, attr); // payload[0..7]=ieee
            }
            break;
        }

        default:
            ESP_LOGD(TAG, "unhandled bridge cmd 0x%02X (%u bytes)",
                     (unsigned)type, (unsigned)len);
            break;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "H2 coordinator boot");

    // Zigbee persists network/binding state in NVS. (H2 has no hal_nvs yet;
    // call the IDF API directly — this is the standard esp-zigbee pattern.)
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(uart_bridge_init(on_bridge_cmd));
    ESP_ERROR_CHECK(zigbee_coordinator_register_report_cb(on_sensor_report));
    ESP_ERROR_CHECK(zigbee_coordinator_register_join_cb(on_device_join));
    ESP_ERROR_CHECK(zigbee_coordinator_init());
    ESP_ERROR_CHECK(zigbee_coordinator_start());
    // app_main returns — the UART bridge and Zigbee stack tasks keep running.
}
