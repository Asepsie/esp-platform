# Hardware Specification v2.0
> **Smart Zone Controller Platform — Dual-MCU Architecture**
> Version: 2.0 | June 2026
> Breaking change from v1.x: ESP32-C6 (Wi-Fi/BACnet) + ESP32-H2 (Zigbee)

---

## 1. Dual-MCU rationale

Espressif's own documentation explicitly recommends against single-chip Wi-Fi + Zigbee:
the ESP32-C6 uses time-division multiplexing on one 2.4GHz RF path. Under Wi-Fi load
(BACnet/SC TLS handshakes, COV notification bursts), Zigbee coordinator packet loss
reaches ~80%. This makes sensor data unreliable in exactly the conditions where the
BMS depends on it most.

Solution: dedicated radio per protocol. Zero contention by design.

---

## 2. Exact part numbers — production references

### ESP32-C6 — primary MCU (Wi-Fi / BACnet / UI / Control)

| SKU | Part number | Flash | Temp | Status | ~Price 10K |
|---|---|---|---|---|---|
| TH-DISPLAY prototype | ESP32-C6-WROOM-1-N4 | 4MB ext SPI | –40 to +85°C | Standard catalogue | $2.20 |
| TH-DISPLAY production | ESP32-C6-WROOM-1-N4 | 4MB ext SPI | –40 to +85°C | Same | $2.20 |
| TH-SEGMENT/HEADLESS proto | ESP32-C6-WROOM-1-N4 | 4MB ext SPI | –40 to +85°C | Standard | $2.20 |
| TH-SEGMENT/HEADLESS prod | ESP32-C6-WROOM-1-H4 | 4MB ext SPI | –40 to +105°C | Espressif sales direct | ~$2.60 |

**Why N4 (4MB) not N8 (8MB):**
Zigbee SDK removed from C6 firmware. Estimated C6 firmware size ~1.6–1.8MB.
OTA A/B partition scheme requires 2× firmware + bootloader + NVS + history.
4MB partition table fits comfortably. Saving vs N8: ~$0.30–0.40 at 10K.

**Why C6 not C61:**
C61 (budget variant) lacks LP core, has smaller ROM (256KB vs 320KB C6),
higher sleep power (+11%), and forces more code into flash due to smaller ROM.
Price saving ~$0.15–0.20. Not worth the tradeoffs for a product lifetime claim.

**Dev board:** ESP32-C6-DevKitC-1-N8 (use N8 for dev — more headroom)

---

### ESP32-H2 — Zigbee coprocessor (802.15.4 only)

| SKU | Part number | Flash | Temp | Status | ~Price 10K |
|---|---|---|---|---|---|
| All SKUs, all environments | ESP32-H2-MINI-1-N4 | 4MB | –40 to +105°C | Standard catalogue | $2.00 |

**Why H2:**
- Purpose-built for 802.15.4. No Wi-Fi radio — contention physically impossible.
- –40 to +105°C as standard part. No special variant, no Espressif sales call required.
- Runs production esp-zigbee-sdk. Same ESP-IDF toolchain as C6 — same CI pipeline.
- Full secure boot (ECC) + AES-256-XTS flash encryption — matches C6 security posture.
- ~$2.00 at 10K. H2 is one of Espressif's lowest-cost modules.

**Why H2 not ESP32-H2-WROOM-07:**
WROOM-07 is a compact module with only 3 GPIOs — insufficient for our debug UART
and status LED. MINI-1 has adequate GPIO exposure for a coprocessor role.

**Dev board:** ESP32-H2-DevKitM-1 (available Digikey/Mouser, ~$10)

---

## 3. Temperature coverage by SKU

| Component | TH-DISPLAY | TH-SEGMENT | TH-HEADLESS |
|---|---|---|---|
| C6-WROOM-1-N4 | –40 to +85°C ✅ | –40 to +85°C (proto) | –40 to +85°C (proto) |
| C6-WROOM-1-H4 | Not needed | –40 to +105°C (prod) | –40 to +105°C (prod) |
| H2-MINI-1-N4 | –40 to +105°C ✅ | –40 to +105°C ✅ | –40 to +105°C ✅ |
| ST7789 LCD | –20 to +70°C ⚠ | N/A | N/A |
| 7-segment display | N/A | –40 to +85°C ✅ | N/A |
| Hongfa HF115F relay | –40 to +85°C ✅ | –40 to +85°C ✅ | –40 to +85°C ✅ |
| PCB FR4 | –40 to +130°C ✅ | –40 to +130°C ✅ | –40 to +130°C ✅ |

**H2 solves the temperature problem for all SKUs as a standard part.**
The LCD remains the only –20°C lower bound on TH-DISPLAY — acceptable
for conditioned-space (office/hotel) deployment.
Production TH-DISPLAY can use wide-temp panel for full –40°C coverage.

---

## 4. Flash memory — risk assessment

Flash endurance concern was raised. This is the complete picture:

| Parameter | C6-WROOM-1-N4 | H2-MINI-1-N4 |
|---|---|---|
| Flash type | External Quad SPI | External Quad SPI |
| Retention | 20 years at 85°C | 20 years at 85°C |
| P/E cycles | 100,000 | 100,000 |
| OTA writes (10yr) | ~40 (quarterly) | ~40 (quarterly) |
| NVS writes (10yr) | ~2,000 (setpoint changes) | ~100 (device table) |
| **Cycle budget used** | **<0.1%** | **<0.1%** |

Flash endurance is not a risk at this workload. 100,000 P/E cycles with
<100 actual writes over product lifetime = comfortable 1000× margin.

The previous concern about 105°C flash variants is resolved differently:
H2 is 105°C standard (no variant needed). For C6 at 105°C, the H4 variant
uses the same flash spec at high-temperature qualification — Espressif has
already validated this combination. No custom flash sourcing required.

---

## 5. UART bridge — hardware interface

### Physical connection C6 ↔ H2
```
C6 GPIO16 (TX) ──────────────── H2 GPIO UART_RX
C6 GPIO17 (RX) ──────────────── H2 GPIO UART_TX
C6 GND ──────────────────────── H2 GND
(optional) C6 GPIO ──────────── H2 EN (C6 can hard-reset H2)
```

Level compatibility: both chips are 3.3V — direct connection, no level shifter.
UART config: 115200 baud, 8N1. No hardware flow control required at this baud rate.
Physical distance on PCB: <5cm — signal integrity not a concern.

### H2 hard reset from C6 (recommended)
Connect C6 GPIO to H2 EN pin via 10kΩ resistor.
Allows C6 to hardware-reset H2 if heartbeat fails.
Also used during H2 OTA flashing (EN toggle for bootloader entry).

---

## 6. Pin map — C6 (updated for dual-MCU)

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

RESERVED
  GPIO24–GPIO30   Internal SPI flash — hardware reserved
  GPIO12–GPIO13   USB-JTAG

SPI2 — ST7789 LCD (TH-DISPLAY only, IO_MUX native pins)
  GPIO6   SCLK
  GPIO7   MOSI
  GPIO18  CS (active low)
  GPIO19  DC
  GPIO20  RST (active low)
  GPIO21  BL (LEDC PWM)

I2C0 — CST816 Touch (TH-DISPLAY only)
  GPIO8   SDA  ⚠ strapping — 10kΩ pull-up
  GPIO9   SCL  ⚠ strapping — 10kΩ pull-up
  GPIO10  INT (touch interrupt)

7-segment (TH-SEGMENT — replaces LCD block)
  GPIO6   SEG_CLK (MAX7219 SPI)
  GPIO7   SEG_DATA
  GPIO18  SEG_CS

RELAY OUTPUTS — Hongfa HF115F via PC817 + 2N7002
  GPIO0   RELAY_HEAT
  GPIO1   RELAY_COOL
  GPIO2   RELAY_FAN
  RC snubber (100Ω + 0.1µF X2) on each AC output

UART BRIDGE — C6 ↔ H2
  GPIO16  BRIDGE_TX → H2 RX
  GPIO17  BRIDGE_RX ← H2 TX

BACnet MS/TP — RS-485 (UART0)              ← NEW
  GPIO3   MSTP_TX  → MAX485 DI
  GPIO4   MSTP_RX  ← MAX485 RO
  GPIO5   RS485_DE → MAX485 DE+RE (tied)   (high = drive bus / TX)

I2C EXPANSION BUS (shared, 400 kHz)        ← NEW
  GPIO8   SDA   (shared with CST816 touch + MCP23017/ADS1115/MCP4728)
  GPIO9   SCL
  GPIO14  MCP23017 INT → safety-DI ISR (fast digital-input path)

H2 CONTROL
  GPIO11  H2_EN (hard reset via 10kΩ) ← NEW

STATUS
  GPIO15  STATUS_LED  ⚠ strapping — 10kΩ pull-down

RADIO (internal — no GPIO)
  Wi-Fi 6    — BACnet/SC transport
  BLE 5      — commissioning (all SKUs)
  802.15.4   — NOT USED on C6 (H2 handles it)

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

### Pin map — H2 (Zigbee coprocessor)

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UART BRIDGE — H2 ↔ C6   (firmware uses UART1; UART0 is the log console)
  H2 GPIO5  UART1_TX → C6 GPIO17 (RX)   ⚠ firmware default — confirm vs schematic
  H2 GPIO4  UART1_RX ← C6 GPIO16 (TX)   ⚠ firmware default — confirm vs schematic

H2 CONTROL
  H2 EN ← C6 GPIO11 via 10kΩ (reset control)

STATUS
  H2 GPIO (1 LED) — Zigbee network status

RADIO (internal — dedicated)
  802.15.4   — Zigbee 3.0 coordinator
  BLE 5.2    — reserved, future use

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

---

## 7. Relay specification (unchanged)

Hongfa HF115F-005-1ZS (5V coil, SPDT, PCB mount)
- 16A / 250VAC, –40 to +85°C, 100K cycles at rated load
- Derating: HVAC control circuit at <3A → estimated >500K cycle life
- RC snubber mandatory: 100Ω + 0.1µF X2 across each AC contact
- Flyback diode 1N4148 across coil
- Price at 10K: ~$0.25–0.30 each

---

## 8. Strapping pins (C6) — PCB requirement

| Pin | Required pull | After boot |
|---|---|---|
| GPIO8 | 10kΩ pull-up | I2C SDA (TH-DISPLAY) |
| GPIO9 | 10kΩ pull-up | I2C SCL (TH-DISPLAY) |
| GPIO15 | 10kΩ pull-down | Status LED |

---

## 9. Power supply

Input: 24VAC (HVAC C-wire)
Output: 3.3V for both C6 and H2 from same rail
Total current: C6 peak 350mA (Wi-Fi TX) + H2 peak 30mA (Zigbee TX) + relays 75mA
→ Specify 500mA minimum, 750mA recommended for margin

H2 can enter deep sleep between Zigbee events: ~15µA sleep current.
C6 runs continuously for BACnet/SC connection maintenance.

---

## 10. Development hardware to buy

| Item | Part | Where | ~Price |
|---|---|---|---|
| C6 dev board | ESP32-C6-DevKitC-1-N8 | Digikey, Mouser | $9 |
| H2 dev board | ESP32-H2-DevKitM-1 | Digikey, Mouser | $10 |
| LCD display | Spotpear ESP32-C6 1.9" (ST7789+CST816) | spotpear.com | $15 |
| Temp/humidity | Sonoff SNZB-02P (Zigbee 3.0) | Amazon, itead.cc | $12 |
| CO₂ sensor | Sonoff SNZB-CO2 (Zigbee 3.0) | Amazon | $35 |
| Dry contact | Sonoff SNZB-04P (Zigbee 3.0) | Amazon, itead.cc | $10 |
| Relay modules ×3 | Pre-built with Hongfa or equivalent | Amazon | $8 |
| Logic analyzer | 24MHz 8-channel clone | Amazon | $15 |
| USB-C cable ×2 | Quality data cable | Any | $10 |
| Jumper wires | M-M + M-F assortment | Any | $8 |
| **Subtotal (base bench)** | | | **~$132** |

Buy 2× of each dev board. One for each side of the UART bridge.
You need both running simultaneously to test the bridge protocol.

### v2.1 expansion peripherals (breakout boards)

Needed to bring up the §12 components on real silicon (currently host-tested only).
All ride the shared I2C bus (GPIO8/9) except RS-485 (UART0) and the RTD front-end (SPI).

| Item | Part / breakout | Bus / addr | Qty | ~Price |
|---|---|---|---|---|
| RS-485 transceiver | MAX485/MAX3485/SP3485 TTL↔RS-485 module | UART0 + DE→GPIO5 | 2 | $2 ea |
| USB↔RS-485 adapter | FTDI/CH340 dongle — 2nd MS/TP node + sniffer | PC side | 1 | $8 |
| Termination resistors | 120Ω ¼W (both bus ends) | RS-485 | 2 | $1 |
| SHT40 sensor | Adafruit SHT40 (#4885) or generic module | I2C 0x44 | 1 | $5 |
| Digital I/O expander | MCP23017 breakout | I2C 0x20/0x21 | 2 | $4 ea |
| 16-bit ADC | ADS1115 breakout | I2C 0x48/0x49 | 2 | $5 ea |
| 12-bit DAC | MCP4728 breakout | I2C 0x60 | 1 | $7 |
| RTD front-end (opt) | MAX31865 breakout + PT1000 probe | SPI | 1 | $16 |
| 0–10 V stage (opt) | MCP6002 op-amp + 1% resistors | — | 1 set | $3 |
| I2C pull-ups | 4.7kΩ ×2 (if not on breakouts) | GPIO8/9 | 1 set | $1 |
| **Subtotal (v2.1 expansion)** | | | | **~$70** |

Detailed prototyping shopping list lives in `docs/SETUP-v2.md` §1.3.

---

## 12. Expansion components (v2.1 — MS/TP + SHT40 + wired I/O)

### Added transceivers / sensors / expanders

| Part | Function | Interface | Temp range | Qty 10k | Notes |
|---|---|---|---|---|---|
| MAX485 / SP3485 | RS-485 transceiver (BACnet MS/TP) | UART0 + DE | –40…+85 °C | ~$0.15 | DE+RE tied to GPIO5 |
| SHT40 | Temp/RH sensor (onboard) | I2C 0x44 | –40…+125 °C | $0.80 | ±0.2 °C, ±1.8 % RH |
| MCP23017 | 16 × digital I/O expander | I2C 0x20/0x21 | –40…+85 °C | $0.65 | INT → GPIO14 (safety DI) |
| ADS1115 | 4-ch 16-bit ADC, PGA | I2C 0x48/0x49 | –40…+85 °C | $0.90 | 128 SPS; PT1000 via differential |
| MCP4728 | 4-ch 12-bit DAC | I2C 0x60 | –40…+85 °C | $1.10 | 0–10 V analog out via op-amp |

Added BOM cost: **+$0.38** (RS-485 transceiver + passives) **+$0.80** (SHT40) on
every board; the MCP/ADS/MCP4728 expanders are fitted only on wired-I/O SKUs.

### I2C address map (shared GPIO8/9 bus @ 400 kHz)

| Address | Device | Driver | Fitted on |
|---|---|---|---|
| 0x15 | CST816 touch | ui (future) | TH-DISPLAY |
| 0x20 | MCP23017 #1 | hal_i2c_expander | wired-I/O |
| 0x21 | MCP23017 #2 | hal_i2c_expander | wired-I/O (2nd) |
| 0x44 | SHT40 | hal_sensor_local | all |
| 0x48 | ADS1115 #1 | hal_i2c_expander | wired-I/O |
| 0x49 | ADS1115 #2 | hal_i2c_expander | wired-I/O (2nd) |
| 0x60 | MCP4728 | hal_i2c_expander | wired-I/O (AO) |

All addresses are distinct → no bus conflict; single shared master bus.

---

## 13. Power budget (three-rail)

```
24VAC ──▶ 5V SMPS (1 A) ──▶ 3.3V LDO (1 A)
```

| Rail | Consumer | Typ draw |
|---|---|---|
| 5 V | relays (3 × HF115F coil ~25 mA) | ~75 mA |
| 5 V | RS-485 transceiver | ~5 mA |
| 5 V | LCD backlight (TH-DISPLAY) | ~120 mA |
| 3.3 V | ESP32-C6 (Wi-Fi TX peak ~350 mA, avg ~80 mA) | ~80 mA avg |
| 3.3 V | ESP32-H2 (802.15.4 TX peak ~30 mA) | ~20 mA avg |
| 3.3 V | SHT40 + MCP23017 + ADS1115 + MCP4728 | ~5 mA |
| 3.3 V | LCD + touch (TH-DISPLAY) | ~40 mA |

3.3 V rail total ≈ 145 mA typical (peak ~480 mA during simultaneous Wi-Fi TX);
the 1 A LDO leaves **~52 % headroom** at typical load and ample peak margin.
**Conclusion: power is not a constraint.** Use a powered supply (not bus-powered
USB) on the bench to avoid Wi-Fi-TX brownouts.

---

## 14. I/O scan timing

Full scan of 47 I/O points completes in **~76 ms (7.6 % of the 1 s control
budget)**. The scan is pipelined inside `control_loop_tick()`:

```
read DI ─▶ start ADC conversion ─▶ run control logic ─▶ read ADC (prev cycle) ─▶ write DO/AO
```

- **Analog inputs are one cycle (≈1 s) stale by design** — the loop uses the
  previous cycle's ADS1115 result while the next conversion runs. Irrelevant for
  HVAC (thermal time constants are minutes). Full 8-channel/16-bit scan at
  128 SPS spans ~8 s; fine for the same reason.
- **Safety DI** uses the MCP23017 INT → **GPIO14** ISR for a **<1 ms** response,
  independent of the 1 s scan cadence.
- Scan duration is published as **BACnet AI instance 304 (Diag-IOScanTime)**.
