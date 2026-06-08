// =============================================================================
// uart_bridge.c — H2 side UART driver + RX task for the C6 <-> H2 bridge.
//
// Responsibilities:
//   * configure UART1 at 115200 8N1 (no flow control)
//   * RX task: byte-sync to SOF, read the rest of each frame, decode it with the
//     shared framing code, and dispatch valid command frames to the callback
//   * TX helpers: encode + write reports / heartbeats (serialized by a mutex)
//   * heartbeat task: emit MSG_HEARTBEAT every 5 s
//
// Framing/CRC and wire types come from uart_bridge_framing.c / _protocol.h.
// =============================================================================
#include "uart_bridge.h"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_check.h"
#include "esp_log.h"

#include <string.h>

// ---------------------------------------------------------------------------
// Hardware configuration (H2 side).
//
// We use UART1, NOT UART0: UART0 is the default log console (see
// sdkconfig.defaults). The hardware spec (docs/hardware/hardware-spec-v2.md §6)
// shows the bridge on "H2 GPIO UART0_TX/RX" but gives no concrete GPIO numbers
// and assumes the console is elsewhere. Until the schematic pins are final,
// these are placeholders — CONFIRM AGAINST THE BOARD before bring-up. They are
// overridable from the build (e.g. -DUART_BRIDGE_TX_GPIO=...).
// ---------------------------------------------------------------------------
#ifndef UART_BRIDGE_PORT
#define UART_BRIDGE_PORT          UART_NUM_1
#endif
#ifndef UART_BRIDGE_TX_GPIO
#define UART_BRIDGE_TX_GPIO       5    // H2 TX -> C6 GPIO17 (RX)   [TODO confirm]
#endif
#ifndef UART_BRIDGE_RX_GPIO
#define UART_BRIDGE_RX_GPIO       4    // H2 RX <- C6 GPIO16 (TX)   [TODO confirm]
#endif

#define UART_BRIDGE_BAUD          115200
#define UART_BRIDGE_RX_BUF        (BRIDGE_MAX_FRAME * 4)  // driver ring buffer
#define UART_BRIDGE_TX_BUF        0                       // 0 = blocking writes

#define UART_BRIDGE_RX_TASK_STACK 4096
#define UART_BRIDGE_RX_TASK_PRIO  6
#define UART_BRIDGE_HB_TASK_STACK 3072
#define UART_BRIDGE_HB_TASK_PRIO  3

// Max time to wait for the remaining bytes of an in-progress frame. At 115200,
// a full 262-byte frame is ~23 ms; 100 ms is generous slack against jitter.
#define UART_BRIDGE_FRAME_TIMEOUT_MS 100

// NACK reason codes (payload byte 2 of MSG_NACK).
#define NACK_REASON_BAD_CRC       0x01

static const char *TAG = "uart_bridge";

static bridge_cmd_cb_t   s_cmd_cb;
static SemaphoreHandle_t s_tx_mutex;
static uint32_t          s_heartbeat_seq;
static uart_bridge_stats_t s_stats;

// --- low-level helpers -------------------------------------------------------

// Read exactly `n` bytes into `buf`, or fail if they don't all arrive within
// the frame timeout. Returns 0 on success, -1 on timeout/error.
static int read_exact(uint8_t *buf, size_t n)
{
    const TickType_t timeout = pdMS_TO_TICKS(UART_BRIDGE_FRAME_TIMEOUT_MS);
    size_t got = 0;
    while (got < n) {
        int r = uart_read_bytes(UART_BRIDGE_PORT, buf + got, n - got, timeout);
        if (r <= 0) {
            return -1; // timeout or driver error mid-frame
        }
        got += (size_t)r;
    }
    return 0;
}

// Encode and write one frame. Serialized so the RX-side NACK path and the TX
// helpers (report / heartbeat, possibly different tasks) never interleave bytes.
static esp_err_t send_frame(bridge_msg_type_t type,
                            const uint8_t *payload, uint16_t payload_len)
{
    uint8_t frame[BRIDGE_MAX_FRAME];
    int frame_len = bridge_frame_encode(type, payload, payload_len,
                                        frame, sizeof(frame));
    if (frame_len < 0) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_tx_mutex, portMAX_DELAY);
    int written = uart_write_bytes(UART_BRIDGE_PORT, frame, (size_t)frame_len);
    if (written == frame_len) {
        s_stats.tx_frames++;
    }
    xSemaphoreGive(s_tx_mutex);

    return (written == frame_len) ? ESP_OK : ESP_FAIL;
}

static void send_nack(uint8_t orig_type, uint8_t reason)
{
    const uint8_t payload[2] = { orig_type, reason };
    (void)send_frame(MSG_NACK, payload, sizeof(payload));
}

// --- tasks -------------------------------------------------------------------

static void rx_task(void *arg)
{
    (void)arg;
    uint8_t frame[BRIDGE_MAX_FRAME];
    uint8_t payload[BRIDGE_MAX_PAYLOAD];

    for (;;) {
        // 1. Sync: read one byte at a time until we see a start-of-frame marker.
        uint8_t sof;
        if (uart_read_bytes(UART_BRIDGE_PORT, &sof, 1, portMAX_DELAY) != 1) {
            continue;
        }
        if (sof != BRIDGE_SOF) {
            continue; // resync; stray byte between frames
        }
        frame[BRIDGE_OFF_SOF] = BRIDGE_SOF;

        // 2. Read MSG_TYPE + LEN (3 bytes) so we know the payload size.
        if (read_exact(&frame[BRIDGE_OFF_TYPE], 3) != 0) {
            s_stats.rx_framing_errors++;
            continue;
        }
        uint16_t payload_len = (uint16_t)frame[BRIDGE_OFF_LEN]
                             | ((uint16_t)frame[BRIDGE_OFF_LEN + 1] << 8);
        if (payload_len > BRIDGE_MAX_PAYLOAD) {
            s_stats.rx_framing_errors++;
            continue; // bogus length; drop and resync on next SOF
        }

        // 3. Read the payload + CRC (payload_len + 2 bytes).
        if (read_exact(&frame[BRIDGE_OFF_PAYLOAD], (size_t)payload_len + 2) != 0) {
            s_stats.rx_framing_errors++;
            continue;
        }

        // 4. Validate and dispatch.
        bridge_msg_type_t type;
        uint16_t got_len = 0;
        bridge_decode_status_t st = bridge_frame_decode(
            frame, (size_t)BRIDGE_FRAME_OVERHEAD + payload_len,
            &type, payload, sizeof(payload), &got_len);

        switch (st) {
        case BRIDGE_DECODE_OK:
            s_stats.rx_frames_ok++;
            if (s_cmd_cb != NULL) {
                s_cmd_cb(type, payload, got_len);
            }
            break;
        case BRIDGE_DECODE_ERR_BAD_CRC:
            s_stats.rx_crc_errors++;
            send_nack(frame[BRIDGE_OFF_TYPE], NACK_REASON_BAD_CRC);
            break;
        default:
            s_stats.rx_framing_errors++;
            break;
        }
    }
}

static void heartbeat_task(void *arg)
{
    (void)arg;
    const TickType_t period = pdMS_TO_TICKS(5000); // spec: every 5 s
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        (void)uart_bridge_send_heartbeat();
        vTaskDelayUntil(&last_wake, period);
    }
}

// --- public API --------------------------------------------------------------

esp_err_t uart_bridge_init(bridge_cmd_cb_t cmd_cb)
{
    s_cmd_cb = cmd_cb;

    s_tx_mutex = xSemaphoreCreateMutex();
    if (s_tx_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const uart_config_t cfg = {
        .baud_rate  = UART_BRIDGE_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(
        uart_driver_install(UART_BRIDGE_PORT, UART_BRIDGE_RX_BUF,
                            UART_BRIDGE_TX_BUF, 0, NULL, 0),
        TAG, "uart_driver_install failed");
    ESP_RETURN_ON_ERROR(uart_param_config(UART_BRIDGE_PORT, &cfg),
                        TAG, "uart_param_config failed");
    ESP_RETURN_ON_ERROR(
        uart_set_pin(UART_BRIDGE_PORT, UART_BRIDGE_TX_GPIO, UART_BRIDGE_RX_GPIO,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE),
        TAG, "uart_set_pin failed");

    if (xTaskCreate(rx_task, "ub_rx", UART_BRIDGE_RX_TASK_STACK, NULL,
                    UART_BRIDGE_RX_TASK_PRIO, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(heartbeat_task, "ub_hb", UART_BRIDGE_HB_TASK_STACK, NULL,
                    UART_BRIDGE_HB_TASK_PRIO, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "bridge up: UART%d TX=%d RX=%d @ %d 8N1",
             UART_BRIDGE_PORT, UART_BRIDGE_TX_GPIO, UART_BRIDGE_RX_GPIO,
             UART_BRIDGE_BAUD);
    return ESP_OK;
}

esp_err_t uart_bridge_send_sensor_report(const bridge_sensor_report_t *report)
{
    if (report == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return send_frame(MSG_SENSOR_REPORT, (const uint8_t *)report, sizeof(*report));
}

esp_err_t uart_bridge_send_device_join(const bridge_device_join_t *device)
{
    if (device == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return send_frame(MSG_DEVICE_JOIN, (const uint8_t *)device, sizeof(*device));
}

esp_err_t uart_bridge_send_heartbeat(void)
{
    uint32_t seq = ++s_heartbeat_seq;
    const uint8_t payload[4] = {
        (uint8_t)(seq & 0xFF),
        (uint8_t)((seq >> 8) & 0xFF),
        (uint8_t)((seq >> 16) & 0xFF),
        (uint8_t)((seq >> 24) & 0xFF),
    };
    return send_frame(MSG_HEARTBEAT, payload, sizeof(payload));
}

void uart_bridge_get_stats(uart_bridge_stats_t *out)
{
    if (out != NULL) {
        *out = s_stats;
    }
}
