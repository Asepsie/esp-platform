# HAL Design — Hardware Abstraction Layer
> **Smart Zone Controller Platform — Architecture Document**
> Version: 1.0 | Status: Authoritative
> Applies to both firmware projects (firmware-c6 and firmware-h2).
> First concrete module: `hal_gpio` (firmware-c6).

---

## 1. Purpose

The HAL is the single seam between application/business logic and the silicon.
Everything above it (control loop, sensor_state, BACnet, UI, UART bridge) is
written against logical, chip-agnostic interfaces; everything `driver/*` lives
below it. This is what makes the firmware **host-unit-testable** and portable
across SKUs and chip revisions.

**Core principle:** application code names *what* it wants (`RELAY_HEAT` on/off),
never *how* or *where* (GPIO0, `driver/gpio.h`).

---

## 2. Non-negotiable boundary rules

From CLAUDE.md, restated here as the HAL contract:

1. **No `driver/*` includes outside the HAL component.** Only the HAL's target
   `.c` files may include `driver/gpio.h`, `driver/i2c_master.h`, etc. Enforced
   in CI by grep (see §9).
2. **All hardware access goes through `hal_*()` functions.** No peripheral
   register or driver handle is ever exposed to a caller.
3. **`hal_pin_map.h` is private to the HAL component.** Physical pin numbers
   never appear in application code or other components.

A violation of any of these is a build-blocking review failure, not a style nit.

---

## 3. Component naming — `bsp`, not `hal` ⚠

ESP-IDF ships a **reserved core component named `hal`**. A project component of
the same name *overrides* it and breaks the build (mbedtls, the peripheral
drivers, and `esp_hw_support` all include headers like `hal/aes_types.h`,
`hal/gpio_hal.h` from the real component).

Therefore the HAL component directory is named **`bsp`** (board support
package) in both firmware projects:

```
firmware-c6/components/bsp/      ← the C6 HAL component
firmware-h2/components/bsp/      ← the H2 HAL component (minimal: UART, GPIO, NVS)
```

Only the **IDF component name** differs. The **API keeps the `hal_` prefix**
(`hal_gpio.h`, `hal_gpio_init()`, …) — that prefix is the HAL boundary marker
the rules in §2 refer to, and it is unchanged.

---

## 4. Directory layout (per HAL module)

```
components/bsp/
├── CMakeLists.txt              ← registers target .c only; sets include scopes
├── include/                    ← PUBLIC logical API (hal_<peripheral>.h)
│   └── hal_gpio.h
├── private/                    ← PRIVATE to the component (PRIV_INCLUDE_DIRS)
│   └── hal_pin_map.h           ← physical pin numbers — never exported
├── hal_gpio.c                  ← TARGET implementation (sole includer of driver/*)
└── mock/                       ← HOST test doubles (not compiled into firmware)
    ├── hal_gpio_mock.h         ← extra test helpers (inspect/reset)
    └── hal_gpio_mock.c         ← array-backed impl of the same public API
```

Rules for each location:

| Location | Visibility | May include | Compiled into firmware? |
|---|---|---|---|
| `include/hal_*.h` | public | `platform_compat.h`, libc | yes (header) |
| `private/*.h` | component-only | libc | yes (header) |
| `hal_*.c` | target impl | `driver/*`, private headers, public header | **yes** |
| `mock/*` | host tests only | public header, libc | **no** (linked by tests) |

The mock directory is intentionally **not** in the component `SRCS`, so it is
never built for the target — it is pulled in only by `tests/host`.

---

## 5. The three layers of a HAL module

### 5.1 Public interface (`include/hal_<peripheral>.h`)

- A logical-ID enum where applicable (e.g. `hal_gpio_id_t`), ending in a
  `_COUNT` sentinel used for range checks and array sizing.
- Functions returning `esp_err_t`, prefixed `hal_<peripheral>_`.
- Includes **`platform_compat.h`** (not `esp_err.h` directly) so the header
  compiles on both target and host — see §6.
- **No** `driver/*` includes. Must compile with plain gcc.

### 5.2 Private pin map (`private/hal_pin_map.h`)

- Maps each logical ID to its physical pin/bus/channel, indexed by the enum.
- Uses **plain integers** (`uint8_t`), not `gpio_num_t`, so it carries no
  `driver/*` dependency — only the target `.c` includes the driver and casts.
- Source of truth for pins is `docs/hardware/hardware-spec-v2.md` §6 (C6) /
  §6 H2 map; the pin map header cites it.

### 5.3 Target implementation (`hal_<peripheral>.c`)

- The **only** file that includes `driver/*`. Translates logical calls to
  ESP-IDF driver calls via the private pin map.
- Uses the **new** ESP-IDF v5.x driver APIs only (never legacy `driver/i2c.h`,
  `driver/mcpwm.h`, …). See §8 for the header/component mapping.
- Keeps a shadow of last-commanded output state where read-back is needed
  (outputs cannot always be read from silicon).

---

## 6. Portability shim — `platform_compat.h`

HAL public headers and logic components need `esp_err_t` (and the store needs a
mutex and a clock) in both target and host builds. These come from
`components/platform/include/platform_compat.h`, which branches on the
IDF-defined `ESP_PLATFORM` macro:

| Facility | Target (`ESP_PLATFORM`) | Host (unit tests) |
|---|---|---|
| `esp_err_t`, `ESP_OK`, `ESP_ERR_*` | `esp_err.h` | stubs (values mirror IDF) |
| `platform_mutex_*` | FreeRTOS mutex | no-op (host tests are single-threaded) |
| `platform_now_ms()` | `esp_timer_get_time()/1000` | `CLOCK_MONOTONIC` |

The host mutex is a no-op: host tests validate *logic*, not locking;
concurrency is exercised on target / in integration tests.
(`platform/` is also the future home of the full timing abstraction required by
RT-08; today it is just this compat shim.)

---

## 7. Error-handling conventions

- Every HAL function returns `esp_err_t` (`hal_*_get` returns its value through
  an out-parameter so the status channel stays free).
- Out-of-range logical id or `NULL` out-pointer → **`ESP_ERR_INVALID_ARG`**.
- Calling a setter before `hal_*_init()` → **`ESP_ERR_INVALID_STATE`**.
- Driver errors are propagated unchanged (return the `esp_err_t` from the
  `driver/*` call); shadow state is updated only on success.
- Application code wraps fatal init with `ESP_ERROR_CHECK()`; runtime calls that
  can fail benignly are checked and logged.

---

## 8. New-driver-API mapping (target implementations)

HAL target `.c` files must use the v5.x component drivers (legacy drivers are
removed in IDF v6.0):

| HAL module | Header | Component (`PRIV_REQUIRES`) |
|---|---|---|
| `hal_gpio` | `driver/gpio.h` | `esp_driver_gpio` |
| `hal_spi` | `driver/spi_master.h` | `esp_driver_spi` |
| `hal_i2c` | `driver/i2c_master.h` | `esp_driver_i2c` |
| `hal_ledc` | `driver/ledc.h` | `esp_driver_ledc` |
| `hal_timer` | `driver/gptimer.h` | `esp_driver_gptimer` |
| `hal_nvs` | `nvs_flash.h` | `nvs_flash` |
| `hal_wifi` | `esp_wifi.h` | `esp_wifi` |
| `hal_ble` | `esp_*` / NimBLE | `bt` |
| `hal_wdt` | `esp_task_wdt.h` | `esp_system` |

ISR-backed HAL code must honour RT-03 (ISR contract): handlers `IRAM_ATTR`, no
allocation, no NVS/flash, only ISR-safe FreeRTOS calls.

---

## 9. Testing model

- Host tests link the **mock** (`mock/hal_<peripheral>_mock.c`), never the
  target `.c`, so they pull in no ESP-IDF and no `driver/*`.
- The mock implements the exact public API plus inspection helpers
  (`hal_*_mock_get_state`, `hal_*_mock_reset_all`) used to assert and to reset
  between tests (call `*_reset_all()` from Unity `setUp()`).
- The mock mirrors target defaults (e.g. all GPIO LOW after init) so behavioural
  tests match the device.

Boundary check (run in CI):

```bash
# Must match nothing outside the HAL component:
grep -rn '#include "driver/' firmware-*/  --include=*.c --include=*.h \
  | grep -v '/components/bsp/'
```

Run host suites via the shared helper:

```bash
scripts/test-host.sh firmware-c6
scripts/test-host.sh firmware-h2
```

---

## 10. Build wiring

Component `CMakeLists.txt` (target):

```cmake
idf_component_register(SRCS "hal_gpio.c"
                       INCLUDE_DIRS "include"        # public API
                       PRIV_INCLUDE_DIRS "private"   # pin map stays internal
                       REQUIRES platform             # esp_err_t shim (public header)
                       PRIV_REQUIRES esp_driver_gpio)# driver — private, not leaked
```

`REQUIRES platform` is public because the public header includes
`platform_compat.h`; the driver is `PRIV_REQUIRES` so dependents never inherit a
`driver/*` include path.

Host test registration (`tests/host/CMakeLists.txt`):

```cmake
add_host_test(test_hal_gpio
    tests/test_hal_gpio.c
    ../../components/bsp/mock/hal_gpio_mock.c)   # mock, not hal_gpio.c
target_include_directories(test_hal_gpio PRIVATE
    ../../components/bsp/include
    ../../components/bsp/mock
    ../../components/platform/include)
```

---

## 11. Doxygen standard

Every public header and every function carries Doxygen:

- File banner: `@file`, `@brief`, and a short rationale; cross-reference rules
  with `@see`.
- Functions: `@brief`, `@param` / `@param[in,out]`, `@return` or `@retval` for
  each documented status.
- Avoid the literal `/*` sequence inside block comments — the IDF build uses
  `-Werror=comment` and will fail (write “the GPIO driver”, not `driver/​*`).

---

## 12. Adding a new HAL module — checklist

1. `include/hal_<p>.h` — logical enum (+ `_COUNT`), `esp_err_t` API, include
   `platform_compat.h`, full Doxygen. No `driver/*`.
2. `private/hal_pin_map.h` — add the module's physical assignments (plain ints),
   cite the hardware spec.
3. `hal_<p>.c` — target impl using the new driver API (§8); only file including
   `driver/*`; shadow output state if read-back is needed.
4. `mock/hal_<p>_mock.{h,c}` — array-backed impl of the API + inspect/reset
   helpers; plain gcc, no ESP-IDF.
5. Add `<p>.c` to component `SRCS` and the driver to `PRIV_REQUIRES`.
6. `tests/host/tests/test_hal_<p>.c` — link the mock; register in the host
   CMake; cover happy path, defaults, and `INVALID_ARG`/`INVALID_STATE`.
7. `scripts/test-host.sh` green + target build green + boundary grep clean.

---

## 13. Reference implementation — `hal_gpio` (firmware-c6)

Logical lines and physical pins (C6 map, hardware-spec §6):

| `hal_gpio_id_t` | GPIO | Role | Default at init |
|---|---|---|---|
| `HAL_GPIO_RELAY_HEAT` | 0 | Heat-call relay | LOW (off) |
| `HAL_GPIO_RELAY_COOL` | 1 | Cool-call relay | LOW (off) |
| `HAL_GPIO_RELAY_FAN` | 2 | Fan relay | LOW (off) |
| `HAL_GPIO_STATUS_LED` | 15 | Status LED (strapping; ext pull-down) | LOW (off) |
| `HAL_GPIO_H2_EN` | 11 | ESP32-H2 enable/reset (active-high = run) | LOW (held in reset) |

API: `hal_gpio_init()`, `hal_gpio_set(id, level)`, `hal_gpio_get(id, &level)`.

**Safe-default policy:** `hal_gpio_init()` drives every line LOW (relays/LED off,
H2 held in reset). Because `H2_EN` defaults LOW, the C6 startup sequence must
explicitly release the H2:

```c
ESP_ERROR_CHECK(hal_gpio_init());
ESP_ERROR_CHECK(hal_gpio_set(HAL_GPIO_H2_EN, true));  // release H2 from reset
```

Tests: `firmware-c6/tests/host/tests/test_hal_gpio.c` (relay set/read, default
low, invalid id, mock reset) — run against `hal_gpio_mock.c`.

---

## 14. Planned HAL modules

| Module | SKU scope | Status |
|---|---|---|
| `hal_gpio` | all | **done** (relays, LED, H2_EN) |
| `hal_spi` | TH-DISPLAY (LCD), TH-SEGMENT (MAX7219) | todo |
| `hal_i2c` | TH-DISPLAY (touch) | todo |
| `hal_ledc` | TH-DISPLAY (backlight PWM) | todo |
| `hal_ble` | all (commissioning) | todo |
| `hal_nvs` / `hal_ota` / `hal_wifi` / `hal_timer` / `hal_wdt` | all | todo |

H2-side `bsp` is minimal: UART (bridge), GPIO (status LED, H2 self), NVS.
