# Thermostat Firmware — Setup, Workflow & Hardware Guide
> For Linux Mint / Fedora. Kernel dev background assumed — no hand-holding.
> Single reference document. Keep it at repo root alongside CLAUDE.md.

---

## 1. Hardware to buy

### 1.1 Primary development hardware

| Item | Part | Where | ~Price |
|---|---|---|---|
| C6 dev board | ESP32-C6-DevKitC-1-N8 (8MB, primary MCU) | Digikey, Mouser, Amazon | $9 |
| H2 dev board | ESP32-H2-DevKitM-1 (Zigbee coprocessor) | Digikey, Mouser | $10 |
| Display | Spotpear ESP32-C6 1.9" LCD (ST7789 + CST816 touch, 172×320, SD slot, LVGL demos) | spotpear.com | $15 |
| Temp/humidity sensor | Sonoff SNZB-02P (Zigbee 3.0, ±0.2°C, ±2%RH, CR2477 4yr battery) | Amazon, itead.cc | $12 |
| CO₂ sensor | Sonoff SNZB-CO2 or Aqara TVOC/CO₂ (Zigbee 3.0) | Amazon | $30–40 |
| Dry contact sensor | Sonoff SNZB-04P (Zigbee door/window = dry contact input) | Amazon, itead.cc | $10 |
| USB-C cable ×2 | Quality USB-C data cables (not charge-only) — one per dev board | Any | $10 |

**Buy 2× of each dev board.** C6 + H2 must run simultaneously to test the UART bridge.
Second pair for the permanent bench setup with relay wiring and sensors.

### 1.2 Bench wiring components

| Item | Notes | ~Price |
|---|---|---|
| Breadboard (full size) | For relay + optocoupler circuit | $5 |
| PC817 optocouplers ×5 | Relay isolation, matches our GPIO drive circuit | $3 |
| 5V relay module ×3 | Heat/Cool/Fan — pre-built modules with flyback diode | $6 |
| Resistors assortment | 330Ω, 1kΩ, 10kΩ for pull-ups and LED drive | $5 |
| Jumper wires M-M + M-F | 40× each | $6 |
| Logic analyzer (optional but strongly recommended) | Saleae Logic 8 or cheap clone (24MHz 8ch) | $15–150 |
| USB current meter | Inline USB-C, measures mA — useful for power budget validation | $10 |

### 1.3 Total
~$145 for full dev bench (includes both C6 and H2 boards ×2).
~$65 if skipping logic analyzer and current meter.

### 1.4 Notes on sensor selection
- All three Sonoff sensors (SNZB-02P, SNZB-CO2, SNZB-04P) are standard Zigbee 3.0.
  They join as ZED (Zigbee End Devices), report standard ZCL clusters,
  and pair cleanly with a Zigbee Coordinator. Validated with Home Assistant / zigbee2mqtt.
- The Spotpear board has the ESP32-C6FH8 (8MB flash) with ST7789 + CST816 already wired
  and LVGL demo code available. Use it for display bringup validation only —
  the standalone DevKitC-1 is your primary firmware target.
- Do NOT buy Tuya-branded CO₂ sensors for dev. Tuya uses manufacturer-specific
  clusters that deviate from ZCL standard. Sonoff and Aqara behave correctly.

---

## 2. System prerequisites

### 2.1 Linux Mint (Ubuntu/Debian base)

```bash
sudo apt update && sudo apt install -y \
    git curl wget \
    python3 python3-pip python3-venv \
    cmake ninja-build \
    gcc g++ build-essential \
    libusb-1.0-0-dev libudev-dev \
    doxygen graphviz \
    cppcheck \
    lcov \
    pkg-config \
    libgcrypt20-dev          # QEMU dependency
```

### 2.2 Fedora

```bash
sudo dnf install -y \
    git curl wget \
    python3 python3-pip \
    cmake ninja-build \
    gcc gcc-c++ \
    libusb1-devel systemd-devel \
    doxygen graphviz \
    cppcheck \
    lcov \
    pkgconf \
    libgcrypt-devel          # QEMU dependency
```

### 2.3 Node.js LTS (Claude Code dependency)

```bash
# Use nvm — avoids package manager version lock
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.7/install.sh | bash
source ~/.bashrc
nvm install --lts
nvm use --lts
node --version   # should be v22.x or later
```

### 2.4 udev rules for ESP32 USB (do this once)

Without this you need sudo for every flash operation.

```bash
# Create udev rule for Espressif USB devices
sudo tee /etc/udev/rules.d/99-espressif.rules << 'EOF'
# CP2102 USB-UART (most ESP32 dev boards)
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", \
    MODE="0666", GROUP="dialout"
# CH340/CH341
SUBSYSTEM=="tty", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="7523", \
    MODE="0666", GROUP="dialout"
# FTDI
SUBSYSTEM=="tty", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6001", \
    MODE="0666", GROUP="dialout"
# ESP32-C6 native USB (JTAG)
SUBSYSTEM=="usb", ATTRS{idVendor}=="303a", ATTRS{idProduct}=="1001", \
    MODE="0666", GROUP="dialout"
EOF

sudo udevadm control --reload-rules && sudo udevadm trigger

# Add yourself to dialout group (log out and back in after this)
sudo usermod -aG dialout $USER
```

---

## 3. ESP-IDF installation

Use v5.5.x — current stable, ESP32-C6 Zigbee and QEMU support mature.
**Keep ESP-IDF entirely in your home directory. Never under /opt or /usr.**

```bash
mkdir -p ~/esp && cd ~/esp

# Clone with submodules — takes 3–5 minutes
git clone --recursive \
    -b v5.5 \
    https://github.com/espressif/esp-idf.git

cd esp-idf

# Install toolchains — RISC-V for both ESP32-C6 and ESP32-H2
# Downloads ~600MB to ~/.espressif/
./install.sh esp32c6,esp32h2     # both targets: C6 (primary) + H2 (Zigbee)

# Verify
source ./export.sh
idf.py --version    # should print ESP-IDF v5.5.x
riscv32-esp-elf-gcc --version   # RISC-V cross-compiler (shared by C6 and H2)
```

### 3.1 Shell integration

Add to `~/.bashrc` (or `~/.zshrc`):

```bash
# ESP-IDF — alias only, don't auto-source (slows shell startup)
alias get_idf=". ~/esp/esp-idf/export.sh"

# Optional: auto-activate when entering an ESP-IDF project
# (add to project .envrc if you use direnv)
```

Usage:
```bash
get_idf        # activates ESP-IDF environment for this terminal session
idf.py build   # now works
```

### 3.2 Enable MCP server feature (ESP-IDF v5.5+ / v6.0)

The ESP-IDF MCP server lets Claude Code call `idf.py` commands via structured
MCP tools instead of raw shell — cleaner error reporting, no environment re-source
overhead per call.

```bash
# Check if available in your version
idf.py mcp-server --help

# If available, add to project .mcp.json (see section 6.3)
```

If not available in v5.5, upgrade to v6.0 stable when released or use the
wrapper script approach in section 5.2.

---

## 4. Claude Code installation and configuration

```bash
npm install -g @anthropic-ai/claude-code

# Verify
claude --version
```

### 4.1 Install the ESP-IDF dev plugin for Claude Code

Gives Claude Code deep knowledge of ESP-IDF v5.x APIs, FreeRTOS patterns,
OTA, and 12 documented gotchas. Auto-activates on ESP-IDF project context.

```bash
cd ~/thermostat
claude
# In Claude Code session:
> Please install the ESP-IDF skill from:
  https://github.com/dropbop/esp-idf-dev-plugin
```

### 4.2 Optional: esp32-claude-workbench

Structured mission/contract workflow for Claude Code ESP32 sessions.
Worth reviewing for patterns even if not installing directly.
https://github.com/agodianel/esp32-claude-workbench

---

## 5. Project setup

### 5.1 Clone and initial structure

```bash
cd ~
git clone <your-repo-url> thermostat
cd thermostat

# Verify architecture docs are present
ls docs/architecture/
# Should show: rt-rules.md  hal-design.md  data-model.md

ls CLAUDE.md   # Claude Code reads this automatically
```

### 5.2 Create the IDF wrapper script

Solves the environment variable persistence problem — Claude Code's bash tool
creates ephemeral shells that lose IDF environment between calls.

```bash
mkdir -p scripts

cat > scripts/idf.sh << 'EOF'
#!/bin/bash
# ESP-IDF wrapper — sources environment then passes all args to idf.py
# Use this instead of calling idf.py directly in all contexts.
source ~/esp/esp-idf/export.sh > /dev/null 2>&1
exec idf.py "$@"
EOF

chmod +x scripts/idf.sh

# Test
./scripts/idf.sh --version
```

### 5.3 Create host test build script

```bash
cat > scripts/test-host.sh << 'EOF'
#!/bin/bash
# Build and run host unit tests (no ESP-IDF, no hardware needed)
set -e
cmake -DTARGET_PLATFORM=host -B build/host -G Ninja
cmake --build build/host
ctest --test-dir build/host --output-on-failure -j$(nproc)
EOF

chmod +x scripts/test-host.sh
```

### 5.4 QEMU setup (for integration tests)

Espressif's ESP32-C6 QEMU fork. Not mainline QEMU.

```bash
# Install via ESP-IDF tools manager (easiest)
get_idf
idf.py --enable-preview install qemu-riscv32

# Verify
~/.espressif/tools/qemu-riscv32/*/bin/qemu-system-riscv32 --version
```

If the above fails (preview feature availability varies by IDF version):

```bash
# Build from source (takes ~10 min)
cd ~/esp
git clone --depth 1 \
    https://github.com/espressif/qemu.git \
    qemu-esp32c6

cd qemu-esp32c6
./configure --target-list=riscv32-softmmu \
            --enable-gcrypt \
            --disable-werror
make -j$(nproc)
sudo make install
```

### 5.5 Static analysis setup

```bash
# cppcheck wrapper
cat > scripts/check.sh << 'EOF'
#!/bin/bash
cppcheck \
    --enable=all \
    --error-exitcode=1 \
    --suppress=missingIncludeSystem \
    --suppress=unusedFunction \
    -I components/bsp/include \
    -I components/sensor_state/include \
    -I components/control/include \
    -I config \
    main/ components/
EOF

chmod +x scripts/check.sh

# clang-tidy (needs compile_commands.json from IDF build)
# Run after: ./scripts/idf.sh build
# Then: run-clang-tidy -p build/ components/ main/
```

### 5.6 Doxygen setup

```bash
cat > Doxyfile << 'EOF'
PROJECT_NAME           = "Thermostat Firmware"
PROJECT_NUMBER         = 1.0.0
OUTPUT_DIRECTORY       = docs/api
INPUT                  = components/bsp/include \
                         components/sensor_state/include \
                         components/control/include \
                         components/bacnet/include \
                         components/zigbee_bridge/include \
                         components/ota/include \
                         components/ui/include
RECURSIVE              = YES
EXCLUDE_PATTERNS       = */private/* */mock/* */test*
EXTRACT_ALL            = NO
WARN_IF_UNDOCUMENTED   = YES
GENERATE_HTML          = YES
GENERATE_LATEX         = NO
HAVE_DOT               = YES
CALL_GRAPH             = YES
EOF

# Add docs/api/ to .gitignore
echo "docs/api/" >> .gitignore
```

---

## 6. Daily workflow

### 6.1 Architecture and design sessions — claude.ai (browser)

Use the claude.ai chat interface for:
- Architecture decisions and tradeoffs (like this conversation)
- Data model design
- Reviewing a module design before implementation
- Generating reference documents

After each session: commit the output to `docs/architecture/`.
These docs are Claude Code's persistent memory across sessions.

### 6.2 Implementation sessions — Claude Code CLI

```bash
cd ~/thermostat
get_idf          # activate ESP-IDF environment

claude           # start Claude Code — reads CLAUDE.md automatically
```

Example session opening:

```
> Read CLAUDE.md and docs/architecture/data-model.md.
  Implement sensor_state.c and sensor_state.h following
  the struct definitions and public API in data-model.md.
  Then write test_sensor_state.c covering:
  - RT-04 mutex contract (no direct struct access)
  - RT-05 mutex hold time budget (1ms)
  - All data_category_t mappings in cluster_map
  Run host unit tests. Fix until green.
```

Claude Code will:
1. Read your actual source files
2. Write `sensor_state.c`, `sensor_state.h`, `test_sensor_state.c`
3. Run `scripts/test-host.sh`
4. Read test failures, fix, rerun
5. Report green

**You review the diff and commit. Never let Claude Code commit directly.**

### 6.3 MCP configuration for the project

Create `.mcp.json` at repo root (Claude Code reads this):

```json
{
  "mcpServers": {
    "esp-idf": {
      "command": "idf.py",
      "args": ["mcp-server"],
      "env": {
        "IDF_PATH": "/home/YOUR_USER/esp/esp-idf"
      }
    }
  }
}
```

Replace `YOUR_USER` with your actual username.
When available, this lets Claude Code call `idf.py build`, `idf.py flash`,
`idf.py size` via structured MCP tools instead of raw shell commands.

### 6.4 Flashing and monitoring — always manual, always your terminal

```bash
# Plug in ESP32-C6-DevKitC-1 via USB-C
# Identify port
ls /dev/ttyUSB* /dev/ttyACM*    # usually /dev/ttyUSB0

# Flash
./scripts/idf.sh flash -p /dev/ttyUSB0 -b 460800

# Monitor (Ctrl+] to exit)
./scripts/idf.sh monitor -p /dev/ttyUSB0

# Combined (build + flash + monitor)
./scripts/idf.sh build flash monitor -p /dev/ttyUSB0
```

**Physical hardware interaction stays in your hands. Not Claude Code's.**

### 6.5 Checking before every commit

```bash
# 1. Host unit tests
./scripts/test-host.sh

# 2. Full IDF build (catches ESP-IDF API errors)
./scripts/idf.sh build

# 3. Flash size sanity check
./scripts/idf.sh size
# App must stay under partition limit (1.9MB per OTA slot)

# 4. Static analysis
./scripts/check.sh

# 5. Git diff review — read every change Claude Code made
git diff
git add -p    # stage interactively, not blindly
git commit
```

### 6.6 Starting a new Claude Code session (context restoration)

Claude Code has no memory between sessions. The CLAUDE.md + architecture docs
carry decisions forward. Start every session with:

```
> Read CLAUDE.md and docs/architecture/.
  Current task: [describe what you're implementing].
  Previous modules completed: [list from CLAUDE.md checklist].
```

Update the `## Module completion checklist` in CLAUDE.md after each module
is done and tested. That's your project status board.

---

## 7. CI/CD pipeline

### 7.1 GitHub Actions — `.github/workflows/ci.yml`

```yaml
name: CI

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]

jobs:
  host-tests:
    name: Host unit tests
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt-get install -y cmake ninja-build gcc cppcheck lcov

      - name: Build and run host tests
        run: |
          cmake -DTARGET_PLATFORM=host -B build/host -G Ninja
          cmake --build build/host
          ctest --test-dir build/host --output-on-failure -j4

      - name: Coverage report
        run: |
          lcov --capture --directory build/host --output-file coverage.info
          lcov --remove coverage.info '/usr/*' '*/test*' \
               --output-file coverage.info
          lcov --list coverage.info

      - name: Static analysis
        run: ./scripts/check.sh

  idf-build:
    name: ESP-IDF build
    runs-on: ubuntu-24.04
    container:
      image: espressif/idf:v5.5
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Build C6 firmware
        run: |
          cd firmware-c6
          idf.py set-target esp32c6
          idf.py build
          idf.py size

      - name: Build H2 firmware
        run: |
          cd firmware-h2
          idf.py set-target esp32h2
          idf.py build
          idf.py size

      - name: Upload firmware artifacts
        uses: actions/upload-artifact@v4
        with:
          name: firmware-unsigned
          path: |
            firmware-c6/build/thermostat-c6.bin
            firmware-h2/build/thermostat-h2.bin

  qemu-integration:
    name: QEMU integration tests
    runs-on: ubuntu-24.04
    needs: idf-build
    if: github.ref == 'refs/heads/main'
    container:
      image: espressif/idf:v5.5
    steps:
      - uses: actions/checkout@v4
      - name: Download firmware
        uses: actions/download-artifact@v4
        with:
          name: firmware-unsigned
      - name: Run QEMU tests
        run: |
          # OTA partition swap, NVS persistence, BACnet/SC stack
          python3 tests/integration/run_qemu.py

  sign-release:
    name: Sign and publish
    runs-on: ubuntu-24.04
    needs: [host-tests, idf-build]
    if: startsWith(github.ref, 'refs/tags/v')
    steps:
      - uses: actions/checkout@v4
      - name: Download firmware
        uses: actions/download-artifact@v4
        with:
          name: firmware-unsigned
      - name: Sign firmware
        env:
          SIGNING_KEY: ${{ secrets.FIRMWARE_SIGNING_KEY }}
        run: |
          echo "$SIGNING_KEY" > /tmp/signing_key.pem
          espsecure.py sign_data \
              --version 2 \
              --keyfile /tmp/signing_key.pem \
              --output thermostat-signed.bin \
              thermostat.bin
      - name: Publish release
        uses: softprops/action-gh-release@v2
        with:
          files: thermostat-signed.bin
```

### 7.2 Branch strategy

```
main        ← always builds, always tests green, signed firmware on tags
develop     ← integration branch, CI runs on push
feature/*   ← module branches, host tests must pass before PR
```

---

## 8. Secure boot and signing key management

### 8.1 Generate signing key (do once, keep offline)

```bash
# On your development machine
get_idf

# Generate RSA-3072 signing key (Secure Boot V2)
espsecure.py generate_signing_key \
    --version 2 \
    --scheme rsa3072 \
    signing_key.pem

# CRITICAL: back up signing_key.pem offline (USB drive, not cloud)
# NEVER commit signing_key.pem to git
# Add to .gitignore immediately
echo "signing_key.pem" >> .gitignore
echo "*.pem" >> .gitignore

# Extract public key for device verification (this one is safe to store)
espsecure.py digest_rsa_public_key \
    --keyfile signing_key.pem \
    --output signing_key_pub.pem
```

### 8.2 sdkconfig.defaults.release — production build flags

```ini
# Secure Boot V2
CONFIG_SECURE_BOOT=y
CONFIG_SECURE_BOOT_V2_ENABLED=y
CONFIG_SECURE_BOOT_SIGNING_KEY="signing_key.pem"

# Flash Encryption
CONFIG_FLASH_ENCRYPTION_ENABLED=y

# Anti-rollback
CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK=y
CONFIG_BOOTLOADER_APP_SEC_VER=1

# Disable debug features
CONFIG_BOOTLOADER_LOG_LEVEL_NONE=y
CONFIG_LOG_DEFAULT_LEVEL_NONE=y
```

Development builds use `sdkconfig.defaults` (no secure boot).
Production builds: `idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.release" build`

---

## 9. QEMU workflow for digital twin

### 9.1 Run firmware in QEMU

```bash
get_idf

# Build for QEMU target
./scripts/idf.sh -DTARGET_PLATFORM=qemu build

# Run in QEMU (no real hardware needed)
qemu-system-riscv32 \
    -nographic \
    -machine esp32c6 \
    -drive file=build/thermostat.bin,if=mtd,format=raw \
    -serial mon:stdio

# Ctrl+A then X to exit QEMU
```

### 9.2 What QEMU validates vs what requires real hardware

| Test | QEMU | Real hardware |
|---|---|---|
| OTA partition swap + rollback | ✅ | ✅ |
| NVS read/write/persistence | ✅ | ✅ |
| FreeRTOS task scheduling | ✅ | ✅ |
| BACnet/SC WebSocket + TLS | ✅ | ✅ |
| Control loop timing | ✅ (approximate) | ✅ (authoritative) |
| Zigbee 802.15.4 radio | ❌ | ✅ |
| LCD SPI output | ❌ | ✅ |
| Relay GPIO | ❌ | ✅ |
| Real-time deadline precision | ❌ | ✅ |
| Power consumption | ❌ | ✅ |

QEMU gives you the software stack, not the hardware peripherals.
All RT deadline validation and peripheral bring-up must be on real hardware.

---

## 10. Recommended VS Code extensions

VS Code is optional — Claude Code is your primary tool — but useful for:
file browsing, git diffs, serial monitor, menuconfig GUI.

```
ms-vscode.cpptools          C/C++ IntelliSense
espressif.esp-idf-extension ESP-IDF: build/flash/monitor/menuconfig GUI
marus25.cortex-debug        OpenOCD debugging (JTAG)
ms-vscode.cmake-tools       CMake integration (for host builds)
eamodio.gitlens             Git diff/blame
```

Install ESP-IDF extension, point it to `~/esp/esp-idf` — gives you:
- `idf.py menuconfig` as a GUI (`Ctrl+Shift+P → ESP-IDF: SDK Configuration Editor`)
- Serial monitor in VS Code terminal
- One-click flash

**Do not use the ESP-IDF extension to write code. Use Claude Code.**

---

## 11. Troubleshooting

### Board not detected
```bash
ls /dev/ttyUSB* /dev/ttyACM*   # nothing?
lsusb                           # check if USB device appears at all
dmesg | tail -20                # check kernel messages on plug-in
groups $USER                    # confirm dialout group membership
# If dialout not shown: log out and back in after step 2.4
```

### IDF build fails on first run
```bash
# Submodules not initialized
cd ~/esp/esp-idf
git submodule update --init --recursive

# Python env issues
./install.sh esp32c6,esp32h2 --reinstall
```

### `get_idf` not found after new terminal
```bash
# Check .bashrc has the alias
grep get_idf ~/.bashrc
# If missing, re-add:
echo 'alias get_idf=". ~/esp/esp-idf/export.sh"' >> ~/.bashrc
source ~/.bashrc
```

### Host tests fail to compile (no ESP-IDF headers)
```bash
# Correct — host tests use mock headers, not ESP-IDF
# Check CMakeLists uses TARGET_PLATFORM=host path
cmake -DTARGET_PLATFORM=host -B build/host
# If CMakeLists not yet created, this is expected — scaffold first
```

### QEMU crash on startup
```bash
# Check QEMU binary matches ESP-IDF version
~/.espressif/tools/qemu-riscv32/*/bin/qemu-system-riscv32 --version
# Must match esp-idf release — if mismatch, reinstall:
idf.py --enable-preview install qemu-riscv32
```

### Flash too full
```bash
./scripts/idf.sh size           # see component breakdown
./scripts/idf.sh size-components # per-component flash usage
# Check CONFIG_COMPILER_OPTIMIZATION in sdkconfig
# Development: -Og, Release: -Os
```

---

## 12. Reference commands — quick lookup

```bash
# Environment
get_idf                                      # activate ESP-IDF

# Build — C6 firmware (primary)
cd firmware-c6/
./scripts/idf.sh set-target esp32c6          # first time only
./scripts/idf.sh build                       # compile
./scripts/idf.sh size                        # flash/RAM usage
./scripts/idf.sh size-components             # per-component breakdown

# Build — H2 firmware (Zigbee coprocessor)
cd firmware-h2/
./scripts/idf.sh set-target esp32h2          # first time only
./scripts/idf.sh build
./scripts/idf.sh size

# Flash and monitor — C6 (first USB port)
cd firmware-c6/
./scripts/idf.sh flash -p /dev/ttyUSB0
./scripts/idf.sh monitor -p /dev/ttyUSB0
./scripts/idf.sh flash monitor -p /dev/ttyUSB0

# Flash and monitor — H2 (second USB port)
cd firmware-h2/
./scripts/idf.sh flash -p /dev/ttyUSB1
./scripts/idf.sh monitor -p /dev/ttyUSB1

# Configuration
./scripts/idf.sh menuconfig                  # ncurses config UI

# Host tests
./scripts/test-host.sh                       # build + run all

# Static analysis
./scripts/check.sh                           # cppcheck
run-clang-tidy -p build/                     # after IDF build

# Docs
doxygen Doxyfile                             # generates docs/api/

# QEMU (C6 only — H2 has no QEMU model, simulated via UART mock)
qemu-system-riscv32 -machine esp32c6 \
    -nographic \
    -drive file=firmware-c6/build/thermostat-c6.bin,if=mtd,format=raw \
    -serial mon:stdio

# OTA signing — C6
espsecure.py sign_data \
    --version 2 \
    --keyfile signing_key.pem \
    --output firmware-c6-signed.bin \
    firmware-c6/build/thermostat-c6.bin

# OTA signing — H2
espsecure.py sign_data \
    --version 2 \
    --keyfile signing_key_h2.pem \
    --output firmware-h2-signed.bin \
    firmware-h2/build/thermostat-h2.bin

# Erase flash — C6 (factory reset)
esptool.py -p /dev/ttyUSB0 erase_flash

# Erase flash — H2
esptool.py -p /dev/ttyUSB1 erase_flash

# Partition table
./scripts/idf.sh partition-table
./scripts/idf.sh partition-table-flash -p /dev/ttyUSB0
```

---

## 13. Project status

**Update this section as modules complete.**

Architecture docs: ✅ committed  
CLAUDE.md: ✅ committed  
SETUP.md: ✅ this file  

Module checklist:
- [ ] Project scaffold (CMakeLists, partitions, sdkconfig, GitHub Actions)
- [ ] `platform/` abstraction layer (host / qemu / target)
- [ ] `config/thermostat_config.h`
- [ ] `sensor_state` + `data_model.h` + `cluster_map`
- [ ] `hal_gpio` (target + mock) + `test_sensor_state.c` green
- [ ] `hal_spi` + `hal_i2c` + `hal_ledc` (target + mock)
- [ ] `hal_nvs` + `hal_ota` + `hal_wifi` (target + mock)
- [ ] `control_loop` + `test_control_loop.c` green
- [ ] `ota_manager` + `ota_transport_menu` + `test_ota_statemachine.c` green
- [ ] `zigbee_bridge` UART client (C6) — the `zigbee_coordinator` + cluster handler live in firmware-h2
- [ ] `bacnet_server` + object model + `test_bacnet_objects.c` green
- [ ] `lvgl_ui` (thermostat screen + OTA progress screen)
- [ ] CI pipeline (build + test + sign + QEMU)
- [ ] Doxygen generation in CI
- [ ] Hardware bringup (relay outputs, LCD, touch)
- [ ] Zigbee pairing with real sensors
- [ ] BACnet/SC end-to-end with test client
- [ ] OTA end-to-end (menu trigger)

---

# APPENDIX A — Windows Setup (Multi-Computer Workflow)

> For working across your Linux laptop AND a Windows machine.
> The project lives in Git — that is the synchronization mechanism.
> Both machines are peers; neither is authoritative except through Git.

## A.1 Multi-computer principle

```
        ┌──────────── GitHub (source of truth) ────────────┐
        │                                                  │
        ▼                                                  ▼
┌─────────────────┐                            ┌─────────────────────┐
│  Linux laptop   │                            │  Windows machine     │
│  Mint / Fedora  │                            │  WSL2 Ubuntu 24.04   │
│  native ESP-IDF │   git push / git pull      │  ESP-IDF in WSL2     │
│  Claude Code    │ ◄────────────────────────► │  Claude Code in WSL2 │
└─────────────────┘                            └─────────────────────┘
```

**Rules for multi-computer sanity:**
- Commit and push before switching machines. Always.
- Pull before starting work on the other machine. Always.
- Never edit the same uncommitted file on both machines — Git can't help with that.
- The architecture docs + CLAUDE.md travel with the repo, so context is identical on both.
- Signing keys (signing_key.pem) are NOT in Git — copy manually via secure USB, store in the same relative path on both machines (and back up offline).

## A.2 Why WSL2, not native Windows

Native Windows ESP-IDF has two real problems for this project: documented path-length build failures on deep component trees, and friction running the host unit tests (which assume a POSIX toolchain). WSL2 gives a real Linux environment at ~95% native build speed, identical to your Mint/Fedora setup. The only cost is USB attachment for flashing, which is one command per session.

**Use WSL2. Treat the Windows machine as "another Linux box that happens to boot Windows first."**

## A.3 WSL2 installation (PowerShell as Administrator)

```powershell
# Install WSL2 with Ubuntu 24.04
wsl --install -d Ubuntu-24.04
wsl --set-default-version 2

# After reboot and Ubuntu first-run (set username/password):
wsl --status          # confirm Version 2, Ubuntu-24.04 default
```

## A.4 Inside WSL2 — identical to your Linux setup

```bash
# Update
sudo apt update && sudo apt upgrade -y

# ESP-IDF prerequisites
sudo apt install -y git wget flex bison gperf \
    python3 python3-pip python3-venv python3-setuptools \
    cmake ninja-build ccache \
    libffi-dev libssl-dev dfu-util libusb-1.0-0-dev libudev-dev \
    doxygen graphviz cppcheck lcov pkg-config libgcrypt20-dev

# Node.js LTS (for Claude Code) via nvm
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.7/install.sh | bash
source ~/.bashrc
nvm install --lts && nvm use --lts

# Claude Code
npm install -g @anthropic-ai/claude-code

# ESP-IDF v5.5 — KEEP INSIDE WSL2 filesystem (~/esp), never under /mnt/c
mkdir -p ~/esp && cd ~/esp
git clone --recursive -b v5.5 https://github.com/espressif/esp-idf.git
cd esp-idf && ./install.sh esp32c6,esp32h2     # both targets

# Shell alias
echo 'alias get_idf=". ~/esp/esp-idf/export.sh"' >> ~/.bashrc
source ~/.bashrc
```

**CRITICAL: Keep the project repo inside the WSL2 Linux filesystem.**
```bash
cd ~                              # WSL2 home — FAST
git clone <your-repo> thermostat  # correct location
# NEVER: cd /mnt/c/Users/...      # Windows filesystem — 10× slower builds
```

## A.5 USB device access (usbipd) — for flashing

Windows does not expose COM ports to WSL2 directly. Use usbipd-win.

```powershell
# PowerShell as Administrator — install once
winget install usbipd

# List USB devices — plug in your ESP32 dev board first
usbipd list

# Bind the device (one-time per device — needs admin)
usbipd bind --busid <BUSID>        # e.g. 1-3

# Attach to WSL2 (each session, or after reboot)
usbipd attach --wsl --busid <BUSID>
```

Then inside WSL2:
```bash
ls /dev/ttyUSB* /dev/ttyACM*       # device now visible
sudo usermod -aG dialout $USER     # once — then log out/in
./scripts/idf.sh flash monitor -p /dev/ttyACM0
```

For dual-MCU work you attach BOTH dev boards:
```powershell
usbipd attach --wsl --busid <C6_BUSID>
usbipd attach --wsl --busid <H2_BUSID>
```
```bash
# Inside WSL2, typically:
# C6 → /dev/ttyACM0   H2 → /dev/ttyACM1   (verify with dmesg | tail)
```

Tip: create a PowerShell script `attach-esp.ps1` that binds+attaches both
boards in one command, so each session start is a single click.

## A.6 VS Code on Windows + WSL2

```
1. Install VS Code on Windows (native)
2. Install extension: "WSL" (ms-vscode-remote.remote-wsl)
3. In WSL2 terminal: cd ~/thermostat && code .
   → VS Code opens connected to WSL2, files live in Linux fs
4. Claude Code runs in the VS Code integrated terminal (which is WSL2)
```

This gives you a native-feeling editor on Windows while everything actually
executes in WSL2. Identical experience to your Linux laptop.

## A.7 Reproduce ESP-IDF install across machines (EIM config)

ESP-IDF's installer now saves an `eim_config.toml` capturing your exact setup.
Copy it to the second machine to reproduce the identical toolchain install —
useful to guarantee both machines build byte-identical firmware.
Located in the ESP-IDF installation directory after install.

## A.8 Multi-computer session checklist

**Leaving a machine:**
```bash
git add -A && git commit -m "wip: <what you did>" && git push
```

**Arriving at the other machine:**
```bash
cd ~/thermostat
git pull
get_idf
claude    # CLAUDE.md re-establishes full context automatically
```

Because CLAUDE.md and docs/architecture/ are in the repo, Claude Code has
identical context on both machines. The only machine-specific things are:
USB port names (ttyACM0 vs COM-attached) and the signing key location.

## A.9 What stays machine-specific (do NOT commit)

| Item | Why | How to sync |
|---|---|---|
| signing_key.pem | Security — never in Git | Secure USB, same path both machines |
| build/ directories | Generated, large | Gitignored — rebuilt per machine |
| /dev port names | Hardware-dependent | Just different per machine, no sync needed |
| ~/esp/esp-idf | Toolchain, huge | Install separately per machine |
| .vscode/settings.json (local paths) | Machine paths | Gitignore local, commit shared settings |
