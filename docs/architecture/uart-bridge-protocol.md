# UART Bridge Protocol
> **Smart Zone Controller Platform — Architecture Document**
> Version: 1.0 | June 2026
> Defines the C6 ↔ H2 internal communication protocol.
> This document is the contract between the two firmware teams.

---

## 1. Overview

The UART bridge is the only communication interface between the ESP32-C6
(primary MCU) and the ESP32-H2 (Zigbee coprocessor). It is a point-to-point,
fixed-framing, half-duplex protocol at 115200 baud.

Neither side calls the other's APIs. Neither knows the other's internal state
beyond what is explicitly communicated in this protocol.

---

## 2. Physical layer

```
UART config: 115200 baud, 8N1, no hardware flow control
C6 TX (GPIO16) ──► H2 RX
C6 RX (GPIO17) ◄── H2 TX
Voltage:  3.3V both sides — direct connection, no level shifter
Distance: <10cm on PCB — no signal integrity concern
```

---

## 3. Frame format

```
┌────────┬──────────┬──────────┬──────────────────┬─────────┐
│ SOF    │ MSG_TYPE │ LEN      │ PAYLOAD          │ CRC16   │
│ 1 byte │ 1 byte   │ 2 bytes  │ 0–256 bytes      │ 2 bytes │
│ 0xAA   │          │ LE uint16│                  │ CCITT   │
└────────┴──────────┴──────────┴──────────────────┴─────────┘
```

- SOF: 0xAA (start of frame marker)
- LEN: payload length only, little-endian uint16
- CRC16: CCITT polynomial 0x1021, covers MSG_TYPE + LEN + PAYLOAD
- Max payload: 256 bytes
- On CRC error: discard frame, increment error counter, send NACK

---

## 4. Message type table

### H2 → C6 (sensor data direction)

| MSG_TYPE | Name | Payload | Notes |
|---|---|---|---|
| 0x01 | SENSOR_REPORT | See §4.1 | Zigbee attribute value |
| 0x02 | DEVICE_JOIN | See §4.2 | New device joined network |
| 0x03 | DEVICE_LEAVE | ieee[8] | Device left or timed out |
| 0x04 | DEVICE_STATUS | ieee[8] + online[1] | Online/offline change |
| 0x30 | HEARTBEAT | sequence[4] | Every 5 seconds |
| 0x31 | NACK | orig_type[1] + reason[1] | Frame rejected |

### C6 → H2 (command direction)

| MSG_TYPE | Name | Payload | Notes |
|---|---|---|---|
| 0x10 | PERMIT_JOIN | duration_s[1] | 0 = close, 254 = open indefinitely |
| 0x11 | POLL_ATTR | ieee[8] + cluster[2] + attr[2] | Request immediate read |
| 0x12 | BIND_DEVICE | ieee[8] + space_id[16] | Associate device to space |
| 0x13 | REMOVE_DEVICE | ieee[8] | Remove from network |
| 0x20 | OTA_START | image_size[4] + sha256[32] | Begin H2 firmware update |
| 0x21 | OTA_DATA | chunk_seq[2] + data[0–128] | Firmware chunk |
| 0x22 | OTA_END | — | Finalize, reboot H2 |
| 0x30 | HEARTBEAT | sequence[4] | Every 5 seconds |

---

## 4.1 SENSOR_REPORT payload

```c
typedef struct __attribute__((packed)) {
    uint8_t  ieee_addr[8];    // Zigbee IEEE address (little-endian)
    uint16_t cluster_id;      // ZCL cluster (e.g. 0x0402 = temperature)
    uint16_t attribute_id;    // ZCL attribute
    uint8_t  data_type;       // ZCL data type tag
    float    value_float;     // Normalized value (°C, %, ppm)
    bool     value_bool;      // For binary attributes
    uint8_t  lqi;             // Link quality 0–255
    uint8_t  battery_pct;     // Battery % (from power config cluster, 0xFF = unknown)
} bridge_sensor_report_t;     // 20 bytes
```

---

## 4.2 DEVICE_JOIN payload

```c
typedef struct __attribute__((packed)) {
    uint8_t  ieee_addr[8];
    uint16_t short_addr;
    uint16_t supported_clusters[8];
    uint8_t  cluster_count;
    char     manufacturer[32];
    char     model[32];
} bridge_device_join_t;       // 91 bytes — fits in 256-byte max
// (8 + 2 + 16 + 1 + 32 + 32 = 91; supported_clusters[8] is 16 bytes, not 8.)
```

---

## 5. Heartbeat and watchdog

Both sides send HEARTBEAT every 5 seconds.
C6 tracks last H2 heartbeat timestamp.
If no heartbeat received for 15 seconds (3 missed):

1. C6 increments `h2_fault_count` in state store
2. C6 asserts H2_EN pin low for 100ms then high (hardware reset)
3. C6 waits 3 seconds for H2 bootup
4. If heartbeat resumes → clear fault, log event to NVS
5. If 3 reset attempts fail → set BACnet alarm on Diag object 302 (`H2-Fault`)

This is the most important reliability mechanism in the dual-chip design.
All Zigbee sensor data becomes stale when H2 faults. The BMS must be
notified immediately via BACnet alarm.

---

## 6. OTA — H2 firmware update flow

```
1. C6 receives OTA package (combined C6 + H2 images)
2. C6 applies own firmware, reboots
3. On C6 startup: check if H2 firmware image is newer than H2 current version
4. If yes:
   a. C6 sends MSG_OTA_START to H2 (image size + SHA256)
   b. H2 acknowledges, enters OTA receive mode
   c. C6 sends MSG_OTA_DATA chunks (128 bytes each)
   d. C6 sends MSG_OTA_END
   e. H2 verifies SHA256, reboots
   f. H2 sends HEARTBEAT with new version in payload
5. C6 confirms version match → OTA complete
```

H2 uses ESP-IDF OTA partition scheme (ota_0/ota_1).
C6 uses ESP32 ROM bootloader stub to write H2 flash over UART
(same protocol as esptool.py — well documented, production-tested).

---

## 7. Testing requirements

### Unit tests (host — no hardware)

```
test_uart_bridge_framing.c:
  - test_frame_encode_sensor_report_correct_crc
  - test_frame_decode_valid_frame_succeeds
  - test_frame_decode_bad_crc_rejected
  - test_frame_decode_truncated_frame_rejected
  - test_frame_decode_wrong_sof_rejected
  - test_heartbeat_sequence_increments

test_h2_watchdog.c (C6 side):
  - test_heartbeat_timeout_triggers_reset
  - test_heartbeat_resume_clears_fault
  - test_three_reset_failures_triggers_bacnet_alarm
```

### Integration test (QEMU — C6 simulates H2 via UART loopback)
- C6 QEMU receives scripted SENSOR_REPORT frames
- State store updates correctly
- BACnet AI objects reflect correct values
- HEARTBEAT timeout triggers alarm object

### Hardware integration test
- Logic analyzer on UART lines validates framing
- H2 pairing with real Sonoff sensor → data appears in BACnet client
- Fault injection: disconnect H2 UART → verify C6 alarm within 15s
- H2 OTA via C6: flash new H2 firmware, confirm version bump

---

## 8. Implementation notes

### C6 side (`components/zigbee_bridge/`)
```c
// zigbee_bridge.h — C6 facing interface (hides UART details)
esp_err_t zigbee_bridge_init(void);
esp_err_t zigbee_bridge_permit_join(uint8_t duration_s);
esp_err_t zigbee_bridge_poll_attribute(const uint8_t *ieee,
                                        uint16_t cluster, uint16_t attr);
bool      zigbee_bridge_is_h2_online(void);
// Sensor data arrives via callback registered at init:
typedef void (*bridge_report_cb_t)(const bridge_sensor_report_t *report);
typedef void (*bridge_join_cb_t)(const bridge_device_join_t *device);
```

### H2 side (`components/uart_bridge/`)
```c
// uart_bridge.h — H2 facing interface
// Commands arrive via a callback registered at init:
typedef void (*bridge_cmd_cb_t)(bridge_msg_type_t type,
                                 const uint8_t *payload, uint16_t len);

esp_err_t uart_bridge_init(bridge_cmd_cb_t cmd_cb);  // cmd_cb may be NULL
esp_err_t uart_bridge_send_sensor_report(const bridge_sensor_report_t *r);
esp_err_t uart_bridge_send_device_join(const bridge_device_join_t *d);
esp_err_t uart_bridge_send_heartbeat(void);

// Link-health counters.
typedef struct {
    uint32_t rx_frames_ok;
    uint32_t rx_crc_errors;      // CRC mismatches dropped (NACK sent)
    uint32_t rx_framing_errors;  // bad SOF / length / mid-frame timeout
    uint32_t tx_frames;
} uart_bridge_stats_t;
void uart_bridge_get_stats(uart_bridge_stats_t *out);
```

Implementation notes:
- `uart_bridge_init()` configures **UART1** (UART0 is the H2 log console),
  starts the RX task, and starts a task that emits `HEARTBEAT` every 5 s.
- On a CRC error the RX task drops the frame and replies with `MSG_NACK`
  (`orig_type`, reason `0x01 = bad CRC`).
- The pure framing/CRC (`bridge_frame_encode` / `_decode` / `bridge_crc16`)
  lives in `uart_bridge_framing.c` and is covered by host unit tests.

The `bridge_sensor_report_t` and `bridge_msg_type_t` types are defined
in the shared header `uart_bridge_protocol.h` — **identical copy in both
firmware projects**. Any change to this file requires simultaneous PR
to both `firmware-c6/` and `firmware-h2/`. This is the versioning risk
to manage carefully.
