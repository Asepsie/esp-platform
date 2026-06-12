# BACnet Server + COV — Implementation Design

> **Smart Zone Controller Platform — Architecture Document**
> Version: 0.1 (design sketch) | Status: Proposed — to be implemented in a
> dedicated Claude Code session
> Scope: firmware-c6 only (the C6 is the northbound MCU; the H2 is Zigbee-only)

This document is the design brief for turning the current BACnet **transport
stub** into a real BACnet **server (device)** with **Change-of-Value (COV)**,
built on an open-source stack. It is written to be self-contained: a future
session should be able to implement from this doc + `CLAUDE.md` + the referenced
source files, without re-deriving the architecture.

---

## 1. Current state (baseline)

What exists today in `firmware-c6/components/bacnet/` (~166 lines):

| File | Role | State |
|---|---|---|
| `include/bacnet_transport.h` | datalink ops vtable (`init`/`send`/`receive`/`cleanup`), `bacnet_addr_t`, `bacnet_server_add_transport()`, transport registry | abstraction only |
| `bacnet_transport.c` | registry holding the active transport set | works |
| `bacnet_transport_mstp.c` | MS/TP transport **stub** over `hal_uart_mstp` | raw bytes — **no framing, no token passing, no addressing** |

What is **missing** (everything that makes it a BACnet device):
- No BACnet protocol stack (no APDU/NPDU, no service handlers).
- No object model (objects are *modeled* in `data_model.h` but never instantiated).
- No Device object → cannot be discovered (`Who-Is`/`I-Am`), read, or written.
- **No COV** — `cov_enabled`/`cov_increment` are struct fields with no engine.
- No BACnet/SC datalink; MS/TP is a stub.

Already-present hooks we will build on:
- `data_model.h` Layer 5 — `bacnet_object_map_entry_t { instance, object_name,
  description, type (`bacnet_obj_type_t`), source_category (`data_category_t`),
  source_space_id, cov_enabled, cov_increment }` and `bacnet_obj_type_t`
  (`OBJ_ANALOG_INPUT/OUTPUT/VALUE`, `OBJ_BINARY_*`, `OBJ_MULTI_STATE_VALUE`).
- `sensor_state` API (the **single source of truth**, mutex-guarded, RT-04):
  - **read binding:** `sensor_state_get_bacnet_value(uint32_t instance, float *out)`
    already maps a BACnet instance → present value.
  - **write binding:** `sensor_state_get_recipe()` / `sensor_state_set_recipe()`
    (setpoints, HVAC/occupancy modes, deadband, CO2/contact overrides).
  - diagnostics: `sensor_state_get_deadline_misses()`, NVS commit counter, etc.
- `thermostat_config.h`: `BACNET_SC_ENABLED`, `BACNET_MSTP_ENABLED`,
  `MSTP_BAUD_DEFAULT` (38400), `MSTP_MAC_ADDRESS` (1).
- `CLAUDE.md` instance allocation: `0–99` space-aggregated, `100–199` equipment
  raw, `200–299` control (setpoints/modes/relays), `300–399` diagnostics.

---

## 2. Goals / non-goals

**Goals**
- A standards-conformant BACnet **device** the BMS can discover, read, write,
  and subscribe to.
- **COV** as a first-class feature (unconfirmed + confirmed notifications).
- Same object model visible on **MS/TP** (RS-485) and, later, **BACnet/SC**
  (Wi-Fi), reusing the existing `bacnet_transport` abstraction.
- Objects bound to `sensor_state` (no duplicate state; RT-04 respected).
- Static allocation (RT-06) — fixed object & subscription tables.

**Non-goals (this phase)**
- BTL certification (design to be BTL-*ready*, certify later).
- Intrinsic reporting / alarming / event enrollment (V2 backlog).
- BACnet routing between networks (single multi-homed device; see §7 decision).
- Segmentation of large APDUs (target small APDUs; RPM kept within one APDU).

**Target device profile (initial):** **B-ASC** (Application-Specific Controller)
plus COV server. Targeted BIBBs:
- DS-RP-B (ReadProperty), DS-RPM-B (ReadPropertyMultiple)
- DS-WP-B (WriteProperty) — setpoints/modes/relay commands
- **DS-COV-B (SubscribeCOV + COV notifications)**
- DM-DDB-B (Who-Is / I-Am), DM-DOB-B (Who-Has / I-Have)
- DM-DCC-B, DM-RD-B (optional: DeviceCommunicationControl, ReinitializeDevice)

---

## 3. Stack selection — open source

**Chosen: `bacnet-stack` (Steve Karg)** — https://github.com/bacnet-stack/bacnet-stack

Rationale:
- The de-facto open-source BACnet stack; mature object/service/COV support,
  portable C89, no dynamic allocation required, used on MCUs.
- Datalinks for MS/TP, BACnet/IP, and **BACnet/SC** (`bsc`) — covers our roadmap.
- Built-in COV machinery (`handler_cov_subscribe`, `handler_cov_task`,
  per-object `*_COV_Detect` / `*_Encode_Value_List`).
- ESP-IDF precedent exists (community ports).

**License — important, see `CLAUDE.md`:** bacnet-stack is **GPL-2.0 *with a
linking exception*** (the "independent modules" clause) that permits linking our
proprietary firmware against the unmodified library. Rules we adopt:
- **Never modify core engine files.** Provide all customization through *port*
  files and our own components.
- Vendor it as a **read-only dependency** (managed component or pinned submodule)
  and **document every touched/port file per PR**.
- Confirm the exact exception text for the pinned version before shipping.

**Alternative considered:** a commercial BACnet/SC stack (~$5K, see
`bom-v2.md` risk register) — keep as a fallback if BACnet/SC TLS/WebSocket on
ESP-IDF proves too costly to port (see §7, Milestone 5).

**Footprint budget (estimate, confirm during M0):** core + handlers + ~20
objects ≈ tens of KB flash, low-KB RAM. **Depends on the C6 partition table
being fixed first** — the C6 is still on the default single-app/2 MB table and
needs the 4 MB OTA-A/B layout before Wi-Fi + BACnet + LVGL push the image toward
the documented ~1.8 MB (tracked separately; this design assumes that lands).

---

## 4. Architecture & layering

```
        ┌─────────────────────────── BMS / BACnet client ───────────────────────────┐
        │             (YABE, BAC0, bacrp/bacwp/bacepics, building controller)         │
        └───────────────▲───────────────────────────────────────────▲────────────────┘
                MS/TP (RS-485)                                  BACnet/SC (Wi-Fi, later)
        ┌───────────────┴───────────────────────────────────────────┴────────────────┐
        │  bacnet_transport registry (EXISTING)  —  bacnet_transport_ops_t per datalink │
        │     mstp ops (real framing/token, M4)              sc ops (WebSocket/TLS, M5)  │
        └───────────────▲───────────────────────────────────────────▲────────────────┘
                        │ datalink glue: bacnet-stack datalink_* ⇄ transport ops (NEW)  │
        ┌───────────────┴─────────────────────────────────────────────────────────────┐
        │  bacnet-stack core (VENDORED, unmodified): APDU/NPDU, TSM, handlers,          │
        │  Device object, object types (ai/ao/av/bi/bo/bv/msv), COV engine             │
        └───────────────▲──────────────────────────────▲─────────────────────────────┘
                 object binding (NEW)            COV glue (NEW)
        ┌───────────────┴──────────────────────────────┴─────────────────────────────┐
        │  bacnet_objects.c — object table from bacnet_object_map; present-value       │
        │  get ← sensor_state_get_bacnet_value(); write → sensor_state_set_recipe()    │
        └───────────────▲─────────────────────────────────────────────────────────────┘
        ┌───────────────┴─────────────────────────────────────────────────────────────┐
        │  sensor_state (EXISTING, single source of truth, mutex/RT-04)                 │
        └───────────────────────────────────────────────────────────────────────────────┘
```

Key principle: **bacnet-stack owns protocol + objects; our code owns the
binding to `sensor_state` and the datalink glue.** No state is duplicated — BACnet
present-values are *projections* of `sensor_state`, refreshed by `bacnet_task`.

---

## 5. Proposed component layout

Extend `firmware-c6/components/bacnet/`:

```
components/bacnet/
├── include/
│   ├── bacnet_transport.h        (EXISTING)
│   ├── bacnet_server.h           NEW — init/start/task API
│   └── bacnet_object_map.h       NEW — object table + lookup (mirrors cluster_map)
├── bacnet_transport.c            (EXISTING)
├── bacnet_transport_mstp.c       (EXISTING stub → real framing in M4)
├── bacnet_transport_sc.c         NEW (M5) — BACnet/SC datalink ops
├── bacnet_server.c               NEW — bacnet_task, stack init, service binding
├── bacnet_objects.c              NEW — instantiate objects, present-value get/set
├── bacnet_object_map.c           NEW — the static object table (the “what objects”)
├── bacnet_cov.c                  NEW — COV update/notify glue around the stack
├── bacnet_datalink_glue.c        NEW — datalink_send_pdu/receive ⇄ transport ops
└── port/                         NEW — bacnet-stack port/config (read-only-ish)
    ├── bacnet-config.h           feature switches (which objects/services/COV)
    └── (dlmstp port hooks, rx queue)
```

Vendoring bacnet-stack: prefer **IDF managed component** if a maintained one
exists at a pinned version; otherwise a **git submodule** under
`components/bacnet/vendor/bacnet-stack/` wrapped by a thin `CMakeLists.txt` that
compiles only the needed core + object + handler sources. Keep `bacnet-config.h`
in `port/` — that is where feature selection happens **without editing core**.

---

## 6. Object model design

### 6.1 The object map table (`bacnet_object_map.c`)
Follow the **`cluster_map` pattern** (one static table, one row per object, pure
C, host-testable). Each row = a `bacnet_object_map_entry_t` (already defined in
`data_model.h`). The table is the single source of "what objects exist".

Mapping by instance range (`CLAUDE.md`):

| Range | Objects | BACnet type | Source / binding | Writable | COV |
|---|---|---|---|---|---|
| 0–99 | space-aggregated temp/RH/CO2, occupancy | AI / BI | `sensor_state_get_bacnet_value(inst)` | no | **yes** |
| 100–199 | per-equipment raw values | AI / BI | `sensor_state_get_bacnet_value(inst)` | no | yes |
| 200–299 | setpoints, deadband | AV / AO | `recipe` field ⇄ `set_recipe` | **yes** | yes |
| 200–299 | HVAC mode, occupancy mode | MSV | `recipe.hvac_mode` ⇄ `set_recipe` | **yes** | yes |
| 200–299 | relay commands / status | BO / BV | control loop output / command | yes (BO) | yes |
| 300–399 | diagnostics (RT misses 300, LQI 301, batt 302, NVS 303, scan 304, H2 status) | AI / BV | diagnostic getters | no | optional |

Device object: instance = configured `BACNET_DEVICE_INSTANCE` (new config),
Vendor ID = `BACNET_VENDOR_ID` (new; use an unregistered placeholder for dev,
register with ASHRAE before production), object-name/description/location.

### 6.2 Present-value binding
- **Read (AI/BI/MSV present-value get):** call
  `sensor_state_get_bacnet_value(instance, &f)` → encode per object type. The
  instance→value map already lives in `sensor_state`, so `bacnet_objects.c`
  stays a thin adapter.
- **Write (AO/AV/MSV/BO WriteProperty):** translate object+value →
  `control_recipe_t` field, then `sensor_state_set_recipe()` (read-modify-write
  via `get_recipe`). Enforce range/units; reject out-of-range with the proper
  BACnet error class/code. Honor `priority array` semantics for commandable
  objects if we expose AO/BO (decision §7).
- **Units & scaling:** set `Units` property per `data_category_t`
  (degrees-Celsius, percent, parts-per-million, no-units). Keep engineering
  units consistent with the Zigbee→`sensor_state` pipeline (already °C/%/ppm).

### 6.3 Refresh model
`bacnet_task` polls `sensor_state` each cycle (e.g. 250 ms) and updates each
object's present-value. This is also the COV change-detection trigger (§ below).
Polling (not eventing) keeps RT-04 clean and avoids `sensor_state` callbacks into
the BACnet task. Cadence is a config constant.

---

## 7. Transport integration & the multi-transport decision

bacnet-stack's `datalink_*` layer is classically **one** datalink (`BACDL_*`).
We want MS/TP **and** SC simultaneously, same objects. Two options:

- **(A) Multiplex behind one virtual datalink (recommended).** Implement
  `bacnet_datalink_glue.c` so `datalink_send_pdu()` fans out to the registered
  `bacnet_transport_ops_t` (route by `bacnet_addr_t`, or broadcast to all), and
  `datalink_receive()` pulls from a unified RX queue fed by every transport's
  receive path. The device is **multi-homed** on one BACnet network number.
  Simplest; matches the existing `bacnet_transport` abstraction.
- **(B) bacnet-stack router pattern** with distinct network numbers per port.
  More standards-correct for two physical networks, more complex.

**Decision needed (see §11):** A vs B. Recommendation: **A** for MS/TP-first
bring-up; revisit B if a site requires distinct BACnet network numbers.

**MS/TP datalink (replaces the stub, M4):** real `dlmstp` — preamble `0x55
0xFF`, frame types (Token, PFM, Reply-PFM, Test, Data-Expecting-Reply, etc.),
header + data CRC, and the **master-node token FSM** with MS/TP timing
(Tturnaround, Tno_token, Tusage_timeout, Treply_timeout). Feed it from
`hal_uart_mstp` (currently raw RX/TX + DE direction). MS/TP timing is the
trickiest RT piece — the FSM must be serviced promptly; design `bacnet_task`
(or a dedicated mstp task) cadence around it. Reuse bacnet-stack's `dlmstp.c`
FSM via a port that calls our `hal_uart_mstp` for byte I/O and DE control.

**BACnet/SC datalink (M5):** bacnet-stack `bsc` over WebSocket + TLS. ESP-IDF
provides mbedTLS and a WebSocket client/server, but bacnet-stack's `bsc`
normally uses libwebsockets — **porting the WebSocket/TLS layer to ESP-IDF is
the single largest task** and is the most likely place to fall back to a
commercial SC stack. Phase it last; the design must not block MS/TP on it.

---

## 8. COV design (the headline feature)

bacnet-stack provides the COV mechanics; we wire them up.

### 8.1 Subscriptions
- Handle **SubscribeCOV** (and optionally SubscribeCOVProperty) via
  `handler_cov_subscribe`. Maintain a **static** subscription table sized by
  `BACNET_MAX_COV_SUBSCRIPTIONS` (config; e.g. 16) — RT-06.
- Each subscription: subscriber address + process id, monitored object,
  confirmed/unconfirmed flag, lifetime (seconds) with expiry + resubscribe.

### 8.2 Change detection
- Per object type, bacnet-stack offers `*_COV_Detect()` and a `Changed` flag.
  After `bacnet_task` refreshes a present-value (§6.3), call the object's COV
  detect; for analog objects it compares delta against **`COV_Increment`**
  (seeded from our `bacnet_object_map.cov_increment`). Binary/MSV report on any
  change.
- `cov_enabled` in the object map gates whether an object participates.

### 8.3 Notification
- Run `handler_cov_task()` (or equivalent) on a fixed cadence
  (`BACNET_COV_TASK_PERIOD_MS`) inside `bacnet_task`. It emits
  **UnconfirmedCOVNotification** (broadcast/unconfirmed subs) and
  **ConfirmedCOVNotification** (confirmed subs, via the TSM with retries).
- Notifications must egress on the transport the subscriber arrived on
  (tracked in the subscription's address). With multiplex option A, the glue
  routes by `bacnet_addr_t`.

### 8.4 Acceptance for COV
A client subscribes to an AI (e.g. zone temperature); changing the underlying
Zigbee value by ≥ `COV_Increment` produces a COV notification within one COV task
period; lifetime expiry stops notifications; resubscribe resumes them. Test on
both unconfirmed and confirmed.

---

## 9. Threading, RT, and persistence

- **`bacnet_task`** (FreeRTOS, priority **below** the RT-01 control task so HVAC
  control always wins; e.g. prio 4 vs control prio 5): per cycle — service
  datalink RX → APDU handler; run TSM timer; run COV task; refresh present-values
  from `sensor_state`; (MS/TP FSM if serviced here). Budget + cadence are config.
- **MS/TP FSM** timing may warrant its own higher-cadence task or tight loop;
  decide in M4 against measured turnaround.
- **RT rules:** RT-02 (no blocking in high-prio — `bacnet_task` is not high-prio,
  but datalink receive must use bounded timeouts), RT-04 (only touch state via
  `sensor_state_*`), RT-06 (static object/subscription/APDU buffers), RT-07
  (feed the task WDT).
- **NVS persistence:** Device instance, object-name overrides, MS/TP MAC/baud,
  and BACnet/SC config (URIs, cert handles). COV subscriptions are runtime
  (not persisted). Reuse `hal_nvs`.
- **Config additions (`thermostat_config.h`):** `BACNET_DEVICE_INSTANCE`,
  `BACNET_VENDOR_ID`, `BACNET_MAX_OBJECTS`, `BACNET_MAX_COV_SUBSCRIPTIONS`,
  `BACNET_APDU_TIMEOUT_MS`, `BACNET_TASK_PERIOD_MS`, `BACNET_COV_TASK_PERIOD_MS`,
  `BACNET_TASK_PRIORITY`.

---

## 10. Testing strategy

- **Host unit tests (pure C, like `cluster_map`/`io_scan` suites):**
  - `bacnet_object_map` — table integrity, instance uniqueness, type/category
    consistency, range bounds.
  - object binding logic — value→encode and write→`recipe` mapping (mock
    `sensor_state`), range rejection.
  - COV change-detection logic — increment threshold crossing, binary/MSV change,
    subscription lifetime/expiry (the parts not buried in stack internals; wrap
    so they’re testable).
- **On-target / integration (real hardware or QEMU + client):**
  - Discovery: `Who-Is` → `I-Am`.
  - `bacrp` / `bacrpm` read of every object; `bacwp` write of setpoints/modes.
  - COV: subscribe (unconfirmed + confirmed), perturb a Zigbee value, observe
    notification; verify increment gating and lifetime.
  - MS/TP on RS-485 with a USB-RS485 dongle as a second node (see SETUP §1.3),
    logic-analyzer frame check.
  - Tools: bacnet-stack CLI utils, **BAC0** (Python) or **YABE** as the client.
- **Conformance:** keep a BTL-style checklist per targeted BIBB; generate an
  **EPICS** (`bacepics`) and diff against intent.

---

## 11. Open decisions (resolve at the start of the implementation session)

1. **Transport multiplexing:** §7 option **A (multi-homed, recommended)** vs **B
   (router with network numbers)**.
2. **Commandable objects:** expose setpoints/relays as **commandable** AO/BO
   (full 16-level priority array + relinquish) or as simpler **AV/BV**?
   Recommendation: AV/BV first (simpler), add priority array if the BMS needs it.
3. **BACnet/SC scope/timing:** port bacnet-stack `bsc` to ESP-IDF WebSocket/TLS
   vs defer vs commercial SC stack. MS/TP-first regardless.
4. **Device profile / BTL:** confirm target (B-ASC + DS-COV-B) and whether BTL
   certification is in scope now (affects strictness of error handling, EPICS).
5. **Vendor ID / device instance** allocation policy (dev placeholder →
   registered before production).
6. **Vendoring:** maintained IDF managed component vs pinned submodule of
   bacnet-stack; confirm license-exception text for the pinned version.
7. **Segmentation:** keep "no segmentation" (constrain RPM responses) or enable?

---

## 12. Milestones (suggested session breakdown)

Each milestone is independently buildable/testable; COV is M3.

- **M0 — Stack integration.** Vendor bacnet-stack; `bacnet-config.h`; compile the
  core + Device object into the C6 target build; `bacnet_server_init/start` +
  `bacnet_task`; respond to **Who-Is** with **I-Am** over the existing transport.
  *Done:* device discoverable; target build green; flash budget measured.
- **M1 — Read-only object model.** `bacnet_object_map.{c,h}` +
  `bacnet_objects.c`; AI/BI/MSV bound to `sensor_state_get_bacnet_value`;
  ReadProperty + ReadPropertyMultiple. Host tests for the map/binding.
  *Done:* client reads all sensor/diag objects with correct values/units.
- **M2 — Writable control objects.** AV/BV (or AO/BO) for setpoints/modes/relays;
  WriteProperty → `set_recipe`; range/units enforcement + BACnet errors.
  *Done:* client writes a setpoint; control loop reacts.
- **M3 — COV.** SubscribeCOV (+ optional SubscribeCOVProperty); static
  subscription table; increment-based detection; Unconfirmed + Confirmed
  notifications; lifetime/resubscribe. Host tests for detection/lifetime.
  *Done:* §8.4 acceptance passes on both notification types.
- **M4 — Real MS/TP.** Replace the stub with framing + master-node token FSM over
  `hal_uart_mstp`; validate on RS-485 with a second node + analyzer.
  *Done:* valid MS/TP on the wire; objects/COV work over MS/TP.
- **M5 — BACnet/SC.** Port `bsc` to ESP-IDF WebSocket/TLS (or commercial
  fallback); same objects/COV over SC; both transports simultaneously per §7.
  *Done:* device reachable over SC and MS/TP at once.
- **M6 — Diagnostics & polish (optional).** Full 300–399 diagnostic objects;
  EPICS; BTL-readiness pass; intrinsic reporting/alarming (V2 backlog).

---

## 13. How to use this doc in a future session

Kickoff prompt for the dedicated session:

> Read `CLAUDE.md`, `docs/architecture/bacnet-server-design.md`,
> `firmware-c6/components/bacnet/`, `firmware-c6/components/sensor_state/`, and
> `data_model.h`. Resolve the §11 open decisions with me, then implement
> **Milestone M0** (vendor bacnet-stack, Device object, Who-Is/I-Am) and stop for
> review. Keep the bacnet-stack core unmodified; put all customization in
> `port/` and our components; document every touched file.

Prerequisite tracked elsewhere: the **C6 partition table** (4 MB, OTA A/B,
history) should be fixed before the firmware grows with Wi-Fi + BACnet + LVGL.

---

## 14. References
- bacnet-stack: https://github.com/bacnet-stack/bacnet-stack (GPL-2.0 + linking exception)
- This repo: `firmware-c6/components/bacnet/` (transport), `…/sensor_state/`
  (state API), `…/data_model/include/data_model.h` (Layer 5 object map types),
  `…/bsp/…/hal_uart_mstp.h` (RS-485), `config/thermostat_config.h`
- `CLAUDE.md` — instance allocation, RT rules, HAL boundary, license rules
- `docs/architecture/uart-bridge-protocol.md`, `docs/architecture/platform-variants.md`
- `docs/hardware/bom-v2.md` — commercial BACnet/SC stack fallback (risk register)
