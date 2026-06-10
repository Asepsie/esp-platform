/**
 * @file zigbee_bridge_io.c
 * @brief C6 UART bridge — target I/O layer (RX task + command TX). Target-only.
 *
 * The FreeRTOS RX task (RT-01: prio 6, 8 KB, static) reads frames from the H2
 * over hal_uart, then hands each complete frame to the core
 * zigbee_bridge_process_frame(). Also implements the command-direction API
 * (permit-join / poll-attribute) and the H2 heartbeat watchdog. Not compiled on
 * host — the mock replaces this layer.
 */
#include "zigbee_bridge.h"
#include "zigbee_bridge_internal.h"
#include "uart_bridge_protocol.h"
#include "hal_uart.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#include "thermostat_config.h"

// RT-01: zigbee_bridge_rx — priority 6, 8 KB stack, statically allocated (RT-06).
#define ZB_RX_TASK_STACK    8192
#define ZB_RX_TASK_PRIO     6
#define ZB_FRAME_TIMEOUT_MS 100    // per-frame inter-byte wait
#define ZB_SYNC_TIMEOUT_MS  1000   // SOF wait; also the watchdog service cadence

static const char *TAG = "zb_bridge";

static StaticTask_t      s_rx_tcb;
static StackType_t       s_rx_stack[ZB_RX_TASK_STACK];
static SemaphoreHandle_t s_tx_mutex;
static bool              s_prev_online;

// --- command TX --------------------------------------------------------------

static esp_err_t send_cmd(bridge_msg_type_t type, const uint8_t *payload,
                          uint16_t len)
{
    uint8_t frame[BRIDGE_MAX_FRAME];
    int flen = bridge_frame_encode(type, payload, len, frame, sizeof(frame));
    if (flen < 0) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(s_tx_mutex, portMAX_DELAY);
    int written = hal_uart_write(frame, (size_t)flen);
    xSemaphoreGive(s_tx_mutex);
    return (written == flen) ? ESP_OK : ESP_FAIL;
}

esp_err_t zigbee_bridge_permit_join(uint8_t duration_s)
{
    const uint8_t payload[1] = { duration_s };
    return send_cmd(MSG_PERMIT_JOIN, payload, sizeof(payload));
}

esp_err_t zigbee_bridge_poll_attribute(const uint8_t *ieee,
                                       uint16_t cluster, uint16_t attr)
{
    if (ieee == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t payload[12];
    memcpy(payload, ieee, 8);
    payload[8]  = (uint8_t)(cluster & 0xFF);
    payload[9]  = (uint8_t)(cluster >> 8);
    payload[10] = (uint8_t)(attr & 0xFF);
    payload[11] = (uint8_t)(attr >> 8);
    return send_cmd(MSG_POLL_ATTR, payload, sizeof(payload));
}

// --- RX task -----------------------------------------------------------------

static int read_exact(uint8_t *buf, size_t n)
{
    size_t got = 0;
    while (got < n) {
        int r = hal_uart_read(buf + got, n - got, ZB_FRAME_TIMEOUT_MS);
        if (r <= 0) {
            return -1; // timeout/error mid-frame
        }
        got += (size_t)r;
    }
    return 0;
}

// Log on the online→offline transition (H2 heartbeat lost).
// Future: assert H2_EN to hard-reset the H2 and re-establish the link.
static void service_h2_watchdog(void)
{
    bool online = zigbee_bridge_is_h2_online();
    if (s_prev_online && !online) {
        ESP_LOGE(TAG, "H2 heartbeat lost (> %d ms) — marking H2 offline",
                 H2_HEARTBEAT_TIMEOUT_MS);
        // TODO: hal_gpio pulse H2_EN to hard-reset the H2.
    }
    s_prev_online = online;
}

static void rx_task(void *arg)
{
    (void)arg;
    uint8_t frame[BRIDGE_MAX_FRAME];

    for (;;) {
        // Sync to SOF. The finite timeout lets us service the H2 watchdog even
        // when the link is silent.
        uint8_t sof;
        if (hal_uart_read(&sof, 1, ZB_SYNC_TIMEOUT_MS) != 1) {
            service_h2_watchdog();
            continue;
        }
        if (sof != BRIDGE_SOF) {
            continue;
        }
        frame[BRIDGE_OFF_SOF] = BRIDGE_SOF;

        if (read_exact(&frame[BRIDGE_OFF_TYPE], 3) != 0) {
            continue;
        }
        uint16_t plen = (uint16_t)frame[BRIDGE_OFF_LEN]
                      | ((uint16_t)frame[BRIDGE_OFF_LEN + 1] << 8);
        if (plen > BRIDGE_MAX_PAYLOAD) {
            continue;
        }
        if (read_exact(&frame[BRIDGE_OFF_PAYLOAD], (size_t)plen + 2) != 0) {
            continue;
        }

        zigbee_bridge_process_frame(frame, (size_t)BRIDGE_FRAME_OVERHEAD + plen);
        service_h2_watchdog();
    }
}

esp_err_t zigbee_bridge_io_start(void)
{
    s_tx_mutex = xSemaphoreCreateMutex();
    if (s_tx_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = hal_uart_init();
    if (err != ESP_OK) {
        return err;
    }
    s_prev_online = false;
    if (xTaskCreateStatic(rx_task, "zb_rx", ZB_RX_TASK_STACK, NULL,
                          ZB_RX_TASK_PRIO, s_rx_stack, &s_rx_tcb) == NULL) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "zigbee_bridge up (RX task prio %d)", ZB_RX_TASK_PRIO);
    return ESP_OK;
}
