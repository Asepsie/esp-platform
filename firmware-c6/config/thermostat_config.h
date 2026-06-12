/**
 * @file thermostat_config.h
 * @brief Single source of truth for firmware-c6 compile-time constants.
 *
 * Every tunable / capacity / pin / timing magic number lives here so there is
 * exactly one place to change them. Components include this (directly or via
 * data_model.h) instead of hard-coding values. Pure preprocessor defines, no
 * includes — safe for both target and host builds.
 *
 * Reachability: added to every component's include path globally from the
 * top-level CMakeLists (`idf_build_set_property(COMPILE_OPTIONS "-I.../config")`)
 * and to the host test include path in tests/host/CMakeLists.txt.
 *
 * @see docs/strategy/CLAUDE-v2.md — thermostat_config.h section.
 */
#ifndef THERMOSTAT_CONFIG_H
#define THERMOSTAT_CONFIG_H

/**
 * @defgroup cfg_platform Platform variant
 * @brief Which MCUs/transports this build targets.
 * @{
 */
#define H2_PRESENT           1   /**< ESP32-H2 coprocessor fitted. */
#define C6_PRESENT           1   /**< ESP32-C6 primary MCU fitted. */
#define H2_MODE_COPROCESSOR  1   /**< H2 mode: Zigbee coprocessor over UART bridge. */
#define H2_MODE_STANDALONE   2   /**< H2 mode: standalone (future). */
#define H2_MODE              H2_MODE_COPROCESSOR  /**< Active H2 mode. */
#define BACNET_SC_ENABLED    1   /**< BACnet/SC northbound enabled. */
#define ZIGBEE_SENSORS       1   /**< Sensor data arrives via Zigbee (through H2). */
#define WIRED_IO             0   /**< No wired analog/digital sensor I/O. */
#define MATTER_ENABLED       0   /**< Matter not enabled. */
/** @} */

/**
 * @defgroup cfg_display Display
 * @brief Display variant selection (SKU).
 * @{
 */
#define DISPLAY_LCD          1   /**< TH-DISPLAY: ST7789 LCD + CST816 touch. */
#define DISPLAY_SEGMENT      2   /**< TH-SEGMENT: 4-digit 7-segment + LEDs. */
#define DISPLAY_NONE         3   /**< TH-HEADLESS: LEDs only. */
#define CONFIG_DISPLAY_TYPE  DISPLAY_LCD  /**< Active display variant. */
/** @} */

/**
 * @defgroup cfg_zigbee Zigbee
 * @brief Layer-1 physical device/attribute capacities.
 * @{
 */
#define MAX_ZB_DEVICES              8   /**< Max simultaneous Zigbee devices. */
#define MAX_ZB_CLUSTERS             8   /**< Max stored attributes per device. */
/** @} */

/**
 * @defgroup cfg_data_model Data model
 * @brief Layer-2/3 capacities (equipment, spaces, bindings).
 * @{
 */
#define MAX_EQUIPMENT               4   /**< Max functional equipment units. */
#define MAX_SPACES                  4   /**< Max spaces in the topology. */
#define MAX_EQUIPMENT_PER_SPACE     4   /**< Max equipment referenced per space. */
#define MAX_BINDINGS_PER_EQUIPMENT  8   /**< Max data bindings per equipment. */
/** @} */

/**
 * @defgroup cfg_control Control loop
 * @brief Control task timing/stack and relay protection.
 * @{
 */
#define CONTROL_LOOP_PERIOD_MS      1000  /**< Control tick period (RT-01 hard 1 s). */
#define CONTROL_LOOP_STACK_SIZE     4096  /**< Control task stack (bytes). */
#define CONTROL_LOOP_PRIORITY       5     /**< Control task FreeRTOS priority. */
#define RELAY_CYCLE_DEBOUNCE_MS     500   /**< Min time between relay state changes. */
/** @} */

/**
 * @defgroup cfg_uart_bridge UART bridge (C6 ↔ H2)
 * @brief C6-side bridge pins/baud and H2 liveness.
 * @{
 */
#define UART_BRIDGE_BAUD            115200  /**< Bridge UART baud rate. */
#define UART_BRIDGE_TX_GPIO         16      /**< C6 bridge TX → H2 RX. */
#define UART_BRIDGE_RX_GPIO         17      /**< C6 bridge RX ← H2 TX. */
#define H2_EN_GPIO                  11      /**< C6 → H2 EN (hard reset, active high = run). */
#define H2_HEARTBEAT_TIMEOUT_MS     15000   /**< Missing-heartbeat → H2 fault (3 missed). */
/** @} */

/**
 * @defgroup cfg_mstp BACnet MS/TP (RS-485)
 * @brief Secondary BACnet datalink over RS-485 (runs alongside BACnet/SC).
 * @{
 */
#define BACNET_MSTP_ENABLED  1      /**< Build/enable the MS/TP transport. */
#define MSTP_BAUD_DEFAULT    38400  /**< Line rate (9600–76800). */
#define MSTP_MAC_ADDRESS     1      /**< MS/TP node MAC address (0–127). */
/** @} */

/**
 * @defgroup cfg_bacnet BACnet device
 * @brief Northbound BACnet server identity + task sizing (design §9).
 *        Vendor ID / device instance are DEV PLACEHOLDERS — register the vendor
 *        ID with ASHRAE and assign a site instance before production.
 * @{
 */
#define BACNET_DEVICE_INSTANCE      12345  /**< Device object instance (dev). */
// Vendor ID / name are bacnet-stack-reserved macros; set at its override point
// (components/bacnet/port/bacnet-config.h) so the stack picks them up directly.
#define BACNET_MODEL_NAME           "SZC-C6"
#define BACNET_TASK_PERIOD_MS       250    /**< Server poll / PV-refresh cadence. */
#define BACNET_TASK_PRIORITY        4      /**< Below control task (5) — HVAC wins. */
#define BACNET_TASK_STACK_SIZE      6144   /**< bacnet_task stack (bytes). */
#define BACNET_MAX_COV_SUBSCRIPTIONS 16    /**< Static COV table size (M3). */
/** @} */

/**
 * @defgroup cfg_i2c I2C expansion bus
 * @brief Shared 400 kHz I2C bus (SHT40, touch, MCP23017/ADS1115/MCP4728).
 * @{
 */
#define PINMAP_I2C_EXPANSION_SDA   8       /**< I2C SDA (shared bus). */
#define PINMAP_I2C_EXPANSION_SCL   9       /**< I2C SCL (shared bus). */
#define I2C_EXPANSION_FREQ_HZ      400000  /**< Fast mode (400 kHz). */
/** @} */

/**
 * @defgroup cfg_io I/O expansion
 * @brief Optional wired I/O expanders (set counts to 0 to disable). Counts are
 *        @c \#ifndef-guarded so test builds can enable them via -D overrides.
 * @{
 */
#ifndef IO_MCP23017_COUNT
#define IO_MCP23017_COUNT       0     /**< MCP23017 16-bit DIO expanders: 0/1/2. */
#endif
#define IO_MCP23017_ADDR_1      0x20
#define IO_MCP23017_ADDR_2      0x21
#ifndef IO_ADS1115_COUNT
#define IO_ADS1115_COUNT        0     /**< ADS1115 4ch 16-bit ADCs: 0/1/2. */
#endif
#define IO_ADS1115_ADDR_1       0x48
#define IO_ADS1115_ADDR_2       0x49
#ifndef IO_MCP4728_COUNT
#define IO_MCP4728_COUNT        0     /**< MCP4728 4ch 12-bit DAC: 0/1. */
#endif
#define IO_MCP4728_ADDR         0x60

#define IO_SCAN_SHT40_INTERVAL  10    /**< Read the local SHT40 every N scan ticks. */
#define IO_SCAN_SAFETY_GPIO     14    /**< MCP23017 INT → GPIO14 (fast safety DI). */
/** @} */

/**
 * @defgroup cfg_nvs NVS
 * @brief Non-volatile storage behaviour.
 * @{
 */
#define HAL_NVS_COALESCE_MS         2000  /**< Defer commit until this idle window. */
/** @} */

/**
 * @defgroup cfg_history History
 * @brief Tier-3 historical ring buffer (per space).
 * @{
 */
#define HISTORY_SAMPLE_INTERVAL_MS  (5 * 60 * 1000)  /**< 5-minute sample cadence. */
#define HISTORY_DEPTH_SAMPLES       2016             /**< ~7 days of samples. */
/** @} */

/**
 * @defgroup cfg_watchdog Watchdog
 * @brief Task watchdog timeout.
 * @{
 */
#define TASK_WDT_TIMEOUT_S          4   /**< TWDT timeout (≈2× control period). */
/** @} */

#endif /* THERMOSTAT_CONFIG_H */
