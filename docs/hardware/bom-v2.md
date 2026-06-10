# Platform BOM & Strategy v2.0
> **Dual-MCU Architecture — ESP32-C6 + ESP32-H2**
> Version: 2.0 | June 2026 | Confidential

---

## 1. Three-SKU BOM at 10K units (USD)

| Line item | TH-DISPLAY | TH-SEGMENT | TH-HEADLESS |
|---|---|---|---|
| ESP32-C6-WROOM-1-N4 (4MB) | $2.20 | $2.20 | $2.20 |
| ESP32-H2-MINI-1-N4 (Zigbee) | $2.00 | $2.00 | $2.00 |
| LCD ST7789 + CST816 touch | $2.90 | — | — |
| 4-digit 7-seg (wide-temp) | — | $0.45 | — |
| Status LEDs ×3 | $0.08 | $0.08 | $0.08 |
| Hongfa HF115F relay ×3 | $0.90 | $0.90 | $0.90 |
| RC snubber ×3 (R+C) | $0.24 | $0.24 | $0.24 |
| PC817 optocoupler ×3 | $0.24 | $0.24 | $0.24 |
| MCP23017 I/O expander | — | $0.65 | $0.65 |
| MAX485/SP3485 RS-485 transceiver (+passives) | $0.38 | $0.38 | $0.38 |
| SHT40 onboard temp/RH sensor | $0.80 | $0.80 | $0.80 |
| Power supply (24VAC→3.3V) | $1.40 | $1.40 | $1.40 |
| Passives + connectors | $1.20 | $1.30 | $1.10 |
| PCB (4-layer FR4, ENIG) | $2.40 | $2.40 | $2.40 |
| PCBA (SMT + THT + AOI) | $2.80 | $2.80 | $2.80 |
| Enclosure (ABS injection) | $2.20 | $1.80 | $1.40 |
| Packaging + mounting | $1.00 | $0.80 | $0.60 |
| **Total BOM** | **$20.74** | **$18.44** | **$17.19** |
| **Fully loaded COGS (~1.8×)** | **~$37** | **~$33** | **~$31** |

The MAX485 RS-485 transceiver and SHT40 are the **v2.1 common delta (+$1.18)**,
fitted on every variant (see `architecture/platform-variants.md` §2 and
`hardware-spec-v2.md` §12). Optional wired-I/O expanders (MCP23017 ×2 / ADS1115 ×2 /
MCP4728) are fit-to-order and not in these base totals.

**Delta vs single-chip architecture:** +$1.80–2.00 per unit (cost of H2).
vs the benefit: elimination of the primary reliability risk of the whole platform.
Net: correct trade unconditionally.

---

## 2. Chip cost breakdown and rationale

### C6: 4MB vs 8MB saving

| Variant | 10K price | Delta |
|---|---|---|
| WROOM-1-N8 (8MB) — previous spec | ~$2.50 | baseline |
| WROOM-1-N4 (4MB) — new spec | ~$2.20 | −$0.30 |

Zigbee SDK (~400–600KB) removed from C6. 4MB confirmed sufficient.
Annual saving at 10K units: $3,000. At 100K: $30,000.

### H2: why it earns its cost

| Benefit | Value |
|---|---|
| Eliminates Wi-Fi/Zigbee contention | Primary reliability risk removed |
| H2 is –40 to +105°C standard | Solves TH-SEGMENT/HEADLESS temp spec — no special ordering |
| H2 has its own secure boot + flash encryption | No additional security hardware needed |
| H2 firmware is thin (~200KB) — 4MB is more than sufficient | No flash sizing risk |
| Same ESP-IDF toolchain | Single CI pipeline, single team skillset |

### Net BOM comparison: dual vs single chip

| Approach | TH-DISPLAY BOM (10K) | Notes |
|---|---|---|
| Single C6-N8 (previous) | $18.74 | RF contention risk, temp limitation |
| Dual C6-N4 + H2 (current) | $20.74 | +$2.00, zero RF contention |

Both rows include the v2.1 common delta (+$1.18: RS-485 MS/TP + SHT40), so the
+$2.00 is purely the cost of the H2.

---

## 3. Volume pricing — TH-DISPLAY representative

| Volume | BOM | COGS | Target ASP | Gross margin |
|---|---|---|---|---|
| 1K | $35 | ~$62 | $120–150 | ~59–60% |
| 10K | $20.74 | ~$37 | $90–120 | ~69–76% |
| 100K | $12.20 | ~$22 | $80–110 | ~73–80% |

Margin profile remains competitive vs incumbents at all volumes.

---

## 4. Competitive comparison (updated)

| Product | BOM est (10K) | Market ASP | Est. margin |
|---|---|---|---|
| **TH-DISPLAY (dual-MCU)** | **$20.74** | **$90–120** | **~77–80%** |
| **TH-SEGMENT (dual-MCU)** | **$18.44** | **$80–110** | **~75–78%** |
| **TH-HEADLESS (dual-MCU)** | **$17.19** | **$70–100** | **~71–77%** |
| Schneider SE8000 | ~$60–80 est | $250–400 | ~75–80% |
| Honeywell T6 Pro commercial | ~$25–35 est | $120–200 | ~75–82% |
| Distech ECB-PTU | ~$50–70 est | $220–350 | ~75–80% |

---

## 5. The strategic number (updated)

**This device at 10K volume costs ~$37 fully loaded (TH-DISPLAY).
At 100K volume: ~$22. Against incumbents at $250–400 ASP.**

The +$2 for the H2 does not change the strategic argument.
It strengthens it: the dual-chip design is what Espressif recommends
for production-grade deployments. It is not a cost-cut. It is the right architecture.

---

## 6. Three-SKU product line (updated)

```
┌─────────────────────────────────────────────────────────────┐
│  TH-DISPLAY  —  wall thermostat, occupant-facing            │
│  C6 (Wi-Fi/BACnet/UI) + H2 (Zigbee)                        │
│  ST7789 LCD + CST816 + LVGL                                 │
│  3 relay outputs                                            │
│  BOM (10K): $20.74 | COGS: ~$37 | ASP: $90–120             │
├─────────────────────────────────────────────────────────────┤
│  TH-SEGMENT  —  zone controller, technician-facing          │
│  C6 + H2 | 4-digit 7-seg + status LEDs                     │
│  3–6 relay outputs + MCP23017 I/O expander                  │
│  Full –40 to +85°C (C6) / –40 to +105°C (H2)               │
│  BOM (10K): $18.44 | COGS: ~$33 | ASP: $80–110             │
├─────────────────────────────────────────────────────────────┤
│  TH-HEADLESS  —  embedded controller, BMS/app-facing        │
│  C6 + H2 | LEDs only — BLE + BACnet/SC configuration       │
│  6–12 DO + 4–8 DI + 4–8 AI + 2–4 AO (with expanders)       │
│  Full –40 to +85°C (C6) / –40 to +105°C (H2)               │
│  BOM (10K): $17.19 | COGS: ~$31 | ASP: $70–100             │
├─────────────────────────────────────────────────────────────┤
│  BUNDLE: TH-DISPLAY + TH-HEADLESS                           │
│  Replaces RP-C + wall sensor                                │
│  Bundle COGS: ~$68 | ASP: $150–200                          │
│  vs RP-C + wall sensor: $300–600 market                     │
└─────────────────────────────────────────────────────────────┘
```

---

## 7. Certification NRE (unchanged from v1.1)

| Certification | Cost | Amortized 10K |
|---|---|---|
| UL 60730 | $25–50K | $2.50–5.00 |
| FCC Part 15 | $8–15K | $0.80–1.50 |
| IC Canada | $5–10K | $0.50–1.00 |
| BTL (BACnet/SC) | $15–30K | $1.50–3.00 |
| IEC 62443-4-2 | $30–80K | $3.00–8.00 |
| CE (Europe) | $20–40K | $2.00–4.00 |
| **Total** | **$103–225K** | **$10.30–22.50** |

Note: FCC certification now covers two RF modules (C6 + H2) on same PCB.
This may require separate or combined radiated emissions testing.
Confirm with test lab before budgeting — estimate may increase $5–10K.

---

## 8. Risk register (updated)

| Risk | Severity | Mitigation |
|---|---|---|
| Wi-Fi / Zigbee RF contention | **Eliminated** | Dual-chip by design |
| ESP32-C6 supply disruption | Medium | Dual-source: C6 → C61 (Wi-Fi only, HAL swap) or NRF9161 |
| ESP32-H2 supply disruption | Low | H2 → ESP32-C6 in Zigbee-only mode (temporary, re-enables contention) |
| Chinese-chip procurement | Low | TSMC-fabbed; document supply chain |
| Relay contact life | Low | HF115F derated <3A HVAC load → >500K cycles |
| 24VAC isolation / UL 60730 | High | Certified AC/DC module (RECOM/Mornsun) |
| BACnet/SC stack maturity | Medium | Commercial stack license ($5K) eliminates risk |
| LCD thermal limit (TH-DISPLAY) | Low-V1 | Conditioned space for V1; wide-temp for production |
| UART bridge reliability | Medium | Hardware reset via GPIO; heartbeat watchdog; CRC framing |
| H2 OTA complexity | Low | ROM bootloader protocol well documented; esptool precedent |
| FCC dual-radio testing | Low-Medium | Budget additional $5–10K; test both radios simultaneously |
| bacnet-stack GPL exposure | Medium | Never modify core engine; document all touched files per PR |
