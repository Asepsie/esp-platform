# Platform Variants

> **Smart Zone Controller Platform — Architecture Document**
> Version: 1.0 | Status: Authoritative
> One dual-MCU hardware base (ESP32-C6 + ESP32-H2); variants differ by UI and
> fitted I/O. All variants share the same firmware components.

---

## 1. Variant matrix

| | **A — TH-DISPLAY** | **B — TH-SEGMENT** | **C — TH-HEADLESS** |
|---|---|---|---|
| UI | ST7789 LCD + CST816 touch | 4-digit 7-seg + LEDs | LEDs only |
| Use case | wall thermostat | mechanical room | VAV / FCU / OEM |
| C6 temp | –20…+70 °C (LCD) | –40…+85/105 °C | –40…+85/105 °C |
| **BACnet/SC (Wi-Fi)** | ✅ | ✅ | ✅ |
| **BACnet MS/TP (RS-485)** | ✅ | ✅ | ✅ |
| Zigbee sensors (via H2) | ✅ | ✅ | ✅ |
| Onboard SHT40 | ✅ | ✅ | ✅ |
| Wired I/O expansion | optional | optional | optional |
| Commissioning | BLE + touch | BLE + buttons | BLE only |

BACnet/SC and MS/TP run **simultaneously** on every variant — the same object
model is visible on both datalinks (see `bacnet_transport.h`). Variant A and
Variant B (and C) all ship with both transports.

---

## 2. Bill of materials (delta from base)

Every variant adds, over the dual-MCU base board:

| Item | Cost (10k) |
|---|---|
| MAX485/SP3485 RS-485 transceiver (+passives) — BACnet MS/TP | **+$0.38** |
| SHT40 onboard temp/RH sensor | **+$0.80** |
| **Common delta** | **+$1.18** |

Per-variant additions:

| Variant | UI parts | Approx UI cost |
|---|---|---|
| A — TH-DISPLAY | ST7789 LCD + CST816 touch | +$6–9 |
| B — TH-SEGMENT | MAX7219 + 7-seg + LEDs | +$1.50 |
| C — TH-HEADLESS | status LEDs only | +$0.10 |

---

## 3. Wired I/O expansion

Optional I2C expanders on the shared GPIO8/9 bus (config `IO_*_COUNT`). Fully
populated, the platform exposes **47 I/O points**:

| Function | Device(s) | Points | Addr | Cost (10k) |
|---|---|---|---|---|
| Digital I/O | 2 × MCP23017 | 32 (16 each) | 0x20, 0x21 | 2 × $0.65 |
| Analog in (16-bit) | 2 × ADS1115 | 8 (4 each) | 0x48, 0x49 | 2 × $0.90 |
| Analog out (12-bit) | 1 × MCP4728 | 4 | 0x60 | $1.10 |
| Relay outputs | onboard | 3 (heat/cool/fan) | — | — |
| **Total** | | **47** | | **~$4.20** |

Scanned by `io_scan` inside the control tick (~76 ms / 7.6 % of the 1 s budget).
Fit only what a deployment needs; default config has all expander counts = 0.

### PT1000 / RTD temperature support

- **ADS1115 differential** (TH-* with ADS fitted): ratiometric PT1000 read on a
  differential channel pair → **±0.05 °C**. No extra part beyond the ADS1115.
- **MAX31865 (SPI)** on TH-SEGMENT / TH-HEADLESS (SPI free of the LCD): dedicated
  RTD front-end → **±0.1 °C + RTD fault detection** (open/short). Preferred where
  the SPI bus is available and fault diagnostics are required.

---

## 4. Firmware component reuse matrix

All variants run the same binary base; UI and fitted-I/O differ by config.

| Component | A (DISPLAY) | B (SEGMENT) | C (HEADLESS) | Notes |
|---|---|---|---|---|
| `sensor_state` / `cluster_map` / `data_model` | ✅ | ✅ | ✅ | core |
| `zigbee_bridge` (+ `hal_uart`) | ✅ | ✅ | ✅ | H2 link |
| `control` (loop + RT task) | ✅ | ✅ | ✅ | core |
| `bsp/hal_gpio` / `hal_nvs` / `hal_timer` / `hal_wdt` | ✅ | ✅ | ✅ | core |
| `hal_uart_mstp` (BACnet MS/TP) | ✅ | ✅ | ✅ | RS-485 |
| `hal_sensor_local` (SHT40) | ✅ | ✅ | ✅ | fallback temp |
| `hal_i2c` + `hal_i2c_expander` | opt | opt | opt | wired I/O |
| `io_scan` | opt | opt | opt | runs if I/O fitted |
| `bacnet` (SC + MS/TP) | ✅ | ✅ | ✅ | both transports |
| `hal_spi` / `ui_main_lcd` | ✅ | — | — | LCD/touch |
| `hal_segment` / `ui_main_segment` | — | ✅ | — | 7-seg |
| `ui_main_none` | — | — | ✅ | LEDs |

`io_scan`, `hal_sensor_local`, and `hal_uart_mstp` are compiled into every
variant; they are inert when the corresponding hardware is not fitted (config
counts = 0 / no transceiver), so there is no per-variant firmware fork.
