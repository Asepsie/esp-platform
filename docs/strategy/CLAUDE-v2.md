# CLAUDE.md — Smart Zone Controller Platform v2.0
> Dual-MCU architecture. Read before every Claude Code session.
> Last updated: June 2026

---

## Architecture overview

Two-chip design as recommended by Espressif for simultaneous Wi-Fi + Zigbee.
Single chip causes ~80% Wi-Fi packet loss when Zigbee coordinator is active.

```
┌─────────────────────────────┐  UART  ┌──────────────────────────────┐
│  ESP32-C6  (primary)        │◄──────►│  ESP32-H2  (Zigbee coprocessor)│
│                             │        │                              │
│  Wi-Fi 6 — BACnet/SC        │        │  802.15.4 only — no Wi-Fi    │
│  BLE 5 — commissioning      │        │  Zigbee 3.0 coordinator      │
│  LVGL UI (TH-DISPLAY)       │        │  Dedicated radio             │
│  Control loop + relays      │        │  –40 to +105°C standard      │
│  OTA manager (both chips)   │        │  Thin firmware               │
│  FreeRTOS / ESP-IDF         │        │  FreeRTOS / ESP-IDF          │
│  4MB flash  –40 to +85°C    │        │  4MB flash  –40 to +105°C    │
│  ESP32-C6-WROOM-1-N4        │        │  ESP32-H2-MINI-1-N4          │
│  ~$2.20 at 10K              │        │  ~$2.00 at 10K               │
└─────────────────────────────┘        └──────────────────────────────┘
        BACnet/SC ↑                              Zigbee ↓
        to BMS                                  to sensors
```

**Three SKUs — same dual-chip hardware:**

| SKU | Display | Use case | C6 temp | H2 temp |
|---|---|---|---|---|
| TH-DISPLAY | ST7789 LCD + CST816 | Wall thermostat | –20 to +70°C | –40 to +105°C |
| TH-SEGMENT | 4-digit 7-seg + LEDs | Mechanical room | –40 to +85°C | –40 to +105°C |
| TH-HEADLESS | LEDs only | VAV/FCU/OEM | –40 to +85°C | –40 to +105°C |

**Two firmware projects — one repo:**
```
thermostat/
├── firmware-c6/    ← primary: BACnet, control, UI, OTA
└── firmware-h2/    ← coprocessor: Zigbee coordinator + UART bridge
```

---

## Chip references — exact part numbers

### ESP32-C6 (primary MCU)
| Purpose | Part number | Flash | Temp | Note |
|---|---|---|---|---|
| Proto + TH-DISPLAY prod | ESP32-C6-WROOM-1-N4 | 4MB ext SPI | –40 to +85°C | Standard catalogue |
| TH-SEGMENT/HEADLESS prod | ESP32-C6-WROOM-1-H4 | 4MB ext SPI | –40 to +105°C | Espressif sales direct |
| Dev board | ESP32-C6-DevKitC-1-N4 | 4MB | –40 to +85°C | Or N8 for dev |

Why not C61: C61 has no LP core, smaller ROM (256KB vs 320KB), higher sleep power. Saving is ~$0.20 — not worth the tradeoffs.
Why 4MB not 8MB: Zigbee SDK removed from C6. Estimated C6 firmware ~1.8MB. 4MB gives OTA A/B + headroom. Flash delta at 10K: ~$0.30–0.40 saved.

### ESP32-H2 (Zigbee coprocessor)
| Purpose | Part number | Flash | Temp | Note |
|---|---|---|---|---|
| All SKUs all environments | ESP32-H2-MINI-1-N4 | 4MB | –40 to +105°C | Standard catalogue |
| Dev board | ESP32-H2-DevKitM-1 | 4MB | –40 to +105°C | Available Digikey/Mouser |

Why H2: purpose-built for 802.15.4. No Wi-Fi radio contention possible by design.
H2 is –40 to +105°C as standard — no special variant. Solves thermal concern for all SKUs.
H2 has full secure boot + AES-256 flash encryption — matches C6 security posture.

---

## UART bridge protocol (C6 ↔ H2)

Fixed framing. Defined once. Both sides tested independently.

```c
// uart_bridge_protocol.h — shared, byte-identical in firmware-c6 and firmware-h2.
// Source of truth: firmware-h2/components/uart_bridge/include/uart_bridge_protocol.h
// Full spec: docs/architecture/uart-bridge-protocol.md
// Frame format: [SOF 0xAA][MSG_TYPE][LEN_16 LE][PAYLOAD...][CRC16 CCITT-FALSE]

typedef enum {
    // H2 → C6 (sensor data direction)
    MSG_SENSOR_REPORT = 0x01, // attribute value from Zigbee device
    MSG_DEVICE_JOIN   = 0x02, // new device joined network
    MSG_DEVICE_LEAVE  = 0x03, // device left or timed out
    MSG_DEVICE_STATUS = 0x04, // online/offline change

    // C6 → H2 (command direction)
    MSG_PERMIT_JOIN   = 0x10, // open/close network for joining
    MSG_POLL_ATTR     = 0x11, // request immediate attribute read
    MSG_BIND_DEVICE   = 0x12, // associate device to space
    MSG_REMOVE_DEVICE = 0x13, // remove from network
    MSG_OTA_START     = 0x20, // begin H2 firmware update
    MSG_OTA_DATA      = 0x21, // firmware chunk
    MSG_OTA_END       = 0x22, // finalize H2 update

    // Bidirectional
    MSG_HEARTBEAT     = 0x30, // liveness check, every 5 s
    MSG_NACK          = 0x31, // frame rejected (orig_type + reason)
} bridge_msg_type_t;
```

UART config: 115200 baud, 8N1, hardware flow control optional.
CRC16-CCITT. Max payload 256 bytes. Heartbeat every 5s — H2 absence triggers alarm.

---

## OTA strategy — both chips from one download

Single OTA binary contains both C6 and H2 firmware images.
C6 downloads, verifies, applies own image, then flashes H2 via UART
using ESP32 ROM bootloader protocol (esptool-style passthrough).

```
Cloud / BMS → C6 OTA download (combined image)
                    │
                    ├─ Apply C6 firmware → reboot C6
                    │
                    └─ Flash H2 via UART → H2 reboots
```

No separate OTA channel for H2. One signed package, one OTA event,
one BACnet notification. Atomic from BMS perspective.

---

## Non-negotiable rules

### HAL boundary (same as before — applies to BOTH firmware projects)
- Never include `driver/*` in application code
- All hardware access through `hal_*()`
- `hal_pin_map.h` private to HAL component

### RT rules (applies to C6 firmware; H2 firmware is simpler)
RT-01 task budget | RT-02 no blocking in high-priority tasks
RT-03 ISR contract | RT-04 state via API only | RT-05 mutex budgets
RT-06 no runtime alloc | RT-07 watchdog | RT-08 abstract timing
RT-09 deadline miss monitoring

### Display type — C6 only
```c
#define DISPLAY_LCD      1   // TH-DISPLAY
#define DISPLAY_SEGMENT  2   // TH-SEGMENT
#define DISPLAY_NONE     3   // TH-HEADLESS
#define CONFIG_DISPLAY_TYPE  DISPLAY_LCD
```

### License
- bacnet-stack GPL+exception: never modify core engine files
- All other deps: Apache 2.0 or MIT

---

## Build commands

### C6 firmware (firmware-c6/)
```bash
get_idf
cd firmware-c6/
./scripts/idf.sh set-target esp32c6
./scripts/idf.sh build
./scripts/idf.sh flash monitor -p /dev/ttyUSB0

# Host unit tests
./scripts/test-host.sh
```

### H2 firmware (firmware-h2/)
```bash
get_idf
cd firmware-h2/
./scripts/idf.sh set-target esp32h2
./scripts/idf.sh build
./scripts/idf.sh flash monitor -p /dev/ttyUSB1   # second USB port

# H2 tests (simpler — Zigbee cluster handler + UART bridge framing)
./scripts/test-host.sh
```

### Combined OTA package (root level)
```bash
./scripts/build-combined-ota.sh    # produces combined-firmware-vX.Y.Z.bin
```

---

## Repository structure

```
thermostat/
├── CLAUDE.md                            ← this file
├── SETUP.md
├── docs/
│   ├── architecture/
│   │   ├── rt-rules.md
│   │   ├── hal-design.md
│   │   ├── data-model.md
│   │   └── uart-bridge-protocol.md      ← NEW
│   ├── hardware/
│   │   └── hardware-spec.md
│   └── strategy/
│       └── bom.md
├── scripts/
│   ├── build-combined-ota.sh            ← NEW: packages both firmwares
│   └── ...
├── firmware-c6/                         ← PRIMARY MCU
│   ├── CLAUDE.md                        ← C6-specific instructions
│   ├── CMakeLists.txt
│   ├── sdkconfig.defaults
│   ├── config/thermostat_config.h
│   ├── main/main.c
│   ├── components/
│   │   ├── bsp/                         ← C6 HAL (named "bsp"; IDF reserves "hal"). hal_* API.
│   │   ├── platform/                    ← timing abstraction
│   │   ├── sensor_state/                ← five-layer data model
│   │   ├── zigbee_bridge/               ← NEW: UART bridge client (C6 side)
│   │   │   ├── include/zigbee_bridge.h
│   │   │   └── zigbee_bridge.c          ← receives sensor data from H2 via UART
│   │   ├── bacnet/
│   │   ├── control/
│   │   ├── ota/
│   │   ├── commissioning/
│   │   └── ui/
│   └── tests/
│       └── host/
│
└── firmware-h2/                         ← ZIGBEE COPROCESSOR
    ├── CLAUDE.md                        ← H2-specific instructions
    ├── CMakeLists.txt
    ├── sdkconfig.defaults
    ├── main/main.c
    ├── components/
    │   ├── bsp/                         ← H2 HAL (named "bsp"; IDF reserves "hal"); minimal: UART, GPIO, NVS
    │   ├── zigbee_coordinator/          ← esp-zigbee-sdk coordinator
    │   │   ├── include/
    │   │   ├── zigbee_coordinator.c
    │   │   └── zigbee_cluster_handler.c
    │   └── uart_bridge/                 ← UART bridge server (H2 side)
    │       ├── include/uart_bridge_protocol.h ← shared protocol (byte-identical both projects)
    │       ├── include/uart_bridge.h    ← H2-facing driver interface
    │       ├── uart_bridge_framing.c    ← encode/decode/CRC (pure, host-tested)
    │       └── uart_bridge.c            ← UART driver + RX task; sends sensor data to C6
    └── tests/
        └── host/
            ├── test_cluster_handler.c
            └── test_uart_bridge_framing.c
```

---

## thermostat_config.h (C6)

```c
#define DISPLAY_LCD 1 | DISPLAY_SEGMENT 2 | DISPLAY_NONE 3
#define CONFIG_DISPLAY_TYPE  DISPLAY_LCD

#define MAX_ZB_DEVICES 8 | MAX_ZB_CLUSTERS 8
#define MAX_EQUIPMENT 4 | MAX_SPACES 4
#define MAX_BINDINGS_PER_EQUIPMENT 8

#define CONTROL_LOOP_PERIOD_MS 1000
#define CONTROL_LOOP_PRIORITY 5
#define RELAY_CYCLE_DEBOUNCE_MS 500
#define RELAY_SNUBBER_REQUIRED 1

#define UART_BRIDGE_BAUD    115200
#define UART_BRIDGE_TX_GPIO 16
#define UART_BRIDGE_RX_GPIO 17
#define H2_HEARTBEAT_TIMEOUT_MS 15000   // 3 missed heartbeats → H2 fault alarm

#define HISTORY_SAMPLE_INTERVAL_MS (5*60*1000)
#define HISTORY_DEPTH_SAMPLES 2016
#define TASK_WDT_TIMEOUT_S 4
```

---

## BACnet instance allocation (unchanged)
```
  0–99:   Space aggregated values
100–199:  Equipment raw values
200–299:  Control (setpoints, modes, relays)
300–399:  Diagnostics (RT misses, Zigbee LQI, battery, H2 heartbeat status)
```

---

## Module completion checklist

### Shared / infrastructure
- [x] Repo structure (thermostat/firmware-c6/ + thermostat/firmware-h2/)
- [x] Shared `uart_bridge_protocol.h` protocol definition
- [x] `docs/architecture/uart-bridge-protocol.md`
- [ ] Combined OTA packaging script

### firmware-h2 (simpler — do first to validate Zigbee)
- [x] H2 project scaffold + sdkconfig
- [ ] `zigbee_coordinator` (coordinator role, network formation)
- [ ] `zigbee_cluster_handler` (attribute → bridge message)
- [x] `uart_bridge` (H2 server side — transport/RX/heartbeat done; command dispatch stubbed)
- [x] H2 `bsp` HAL — `hal_uart` + `hal_gpio` (status LED) + host mocks/tests; `uart_bridge.c` now goes through `hal_uart` (the `driver/uart.h` boundary violation is fixed). NVS TBD.
- [ ] `test_cluster_handler.c` green
- [x] `test_uart_bridge_framing.c` green
- [ ] Zigbee pairing: Sonoff SNZB-02P on real hardware
- [ ] UART bridge integration with C6

### firmware-c6 (build on validated H2)
- [x] C6 project scaffold + sdkconfig (no Zigbee in REQUIRES)
- [~] `platform/` (host/target compat shim `platform_compat.h` done; `hal_timer` provides the RT-08 timing abstraction; QEMU variant TBD)
- [x] `config/thermostat_config.h` — single source of compile-time constants; consumed by data_model/control/bsp; global `-I config` (target) + host include
- [x] `sensor_state` + `data_model.h` + `cluster_map`
- [ ] `zigbee_bridge` (C6 client — receive H2 UART messages → state store)
- [x] `hal_gpio` (relays, LED) + mock + tests green — component dir is `components/bsp/` (see note)
- [ ] `hal_spi` + `hal_i2c` + `hal_ledc` (DISPLAY_LCD)
- [ ] `hal_segment` (DISPLAY_SEGMENT)
- [ ] `hal_ble` (commissioning)
- [~] `hal_nvs` ✓ (write-coalescing, commit counter → BACnet AI 303, corruption recovery) · `hal_timer` ✓ (RT-08; deterministic sim-clock mock) · `hal_wdt` ✓ (RT-07; `init(timeout_s)`) — `control_task` drives both (TWDT timeout from `TASK_WDT_TIMEOUT_S`); target + mock + host tests green · `hal_ota` · `hal_wifi` (remaining)
- [x] `control_loop` + tests green (relay hysteresis, modes, dry-contact lockout) + 1 Hz RT-01 control task (`control_task.c`)
- [ ] `ota_manager` + `ota_transport_menu` + H2 flashing via UART + tests green
- [ ] `bacnet_server` + object model + tests green
- [ ] `commissioning_ble`
- [ ] `ui_main_lcd` | `ui_main_segment` | `ui_main_none`
- [ ] `ui_ota`

### CI/CD
- [ ] GitHub Actions: both firmware builds in same pipeline
- [ ] Host tests: both firmware test suites
- [ ] QEMU: C6 integration (H2 side simulated via UART mock)
- [ ] Sign + combined OTA package on release tag
- [ ] Doxygen: both firmware projects → merged API docs

### Hardware validation
- [ ] Relay + snubber circuit
- [ ] UART bridge C6↔H2 at 115200 (logic analyzer validation)
- [ ] H2 heartbeat timeout → C6 alarm relay behavior
- [ ] LCD bringup (TH-DISPLAY)
- [ ] 7-segment bringup (TH-SEGMENT)
- [ ] 24VAC power isolation
- [ ] Zigbee pairing: SNZB-02P + CO2 + SNZB-04P
- [ ] BACnet/SC end-to-end
- [ ] OTA end-to-end (both chips)
- [ ] Secure boot both chips

### V2 backlog
- [ ] `ota_transport_bacnet.c`
- [ ] `ota_transport_https.c`
- [ ] BACnet AES (alarm/event services) — BTL required
- [ ] MCP23017 I/O expander (TH-SEGMENT / TH-HEADLESS)
- [ ] Wide-temp LCD for TH-DISPLAY production
