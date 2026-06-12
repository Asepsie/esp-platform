/**
 * @file bacnet-config.h
 * @brief Port-layer feature selection for the vendored bacnet-stack.
 *
 * This is the ONE place where we customize bacnet-stack — the core under
 * vendor/bacnet-stack/ is never edited (GPL-2.0 + linking exception: customize
 * via the port, link the unmodified library). The stack's own
 * vendor/.../src/bacnet/config.h pulls this file in when the build defines
 * `-DBACNET_CONFIG_H`; see components/bacnet/CMakeLists.txt.
 *
 * Keep this self-contained (no project headers): it is included very early,
 * before most of the stack. Runtime sizing that belongs to the application
 * (device instance, COV table size, task cadence) lives in
 * config/thermostat_config.h and is consumed by our own .c files, not here.
 */
#ifndef BACNET_CONFIG_H_PORT
#define BACNET_CONFIG_H_PORT

/* --- Datalink ---------------------------------------------------------------
 * No BACDL_* is defined: we do NOT compile the stack's datalink.c dispatcher.
 * Instead bacnet_datalink_glue.c provides datalink_*() and fans out to the
 * existing bacnet_transport registry (multi-homed, design §7 option A). MAX_MPDU
 * then falls back to the stack's generic MAX_HEADER+MAX_PDU.
 */

/* --- APDU / transaction sizing (RT-06: static) ------------------------------ */
#define MAX_APDU 480 /* MS/TP-class APDU; fits our small RPM responses. */
#define MAX_TSM_TRANSACTIONS \
    1 /* We originate no confirmed requests in M0–M2. */

/* --- Footprint / behaviour -------------------------------------------------- */
#define BACAPP_MINIMAL /* compile only the application tags we use */
#define BACAPP_TIMESTAMP
#define BACNET_STACK_DEPRECATED_DISABLE /* no deprecated shims */
#define PRINT_ENABLED 0 /* no stdio in the stack */
#define BACNET_BIG_ENDIAN 0 /* ESP32-C6 is little-endian */
#define BACNET_PROTOCOL_REVISION 16

/* --- Device identity (DEV PLACEHOLDERS) -------------------------------------
 * These are bacnet-stack-reserved macros (guarded defaults in its config.h);
 * defining them here, the stack's override point, makes them the device's
 * compile-time vendor identity. Register the vendor ID with ASHRAE before
 * production. The device instance + model name stay in thermostat_config.h.
 */
#define BACNET_VENDOR_ID 555 /* UNREGISTERED — dev only */
#define BACNET_VENDOR_NAME "Smart Zone Controller"

/* --- Object types compiled into the configurable Device object --------------
 * basic/server/bacnet_device.c gates each object type on these. Enable exactly
 * the types we instantiate; the matching ai.c/bi.c/bo.c are in the build list.
 * Writable control types (AV/BV/MSV) arrive with M2 (WriteProperty → recipe).
 */
#define CONFIG_BACNET_BASIC_OBJECT_DEVICE
#define CONFIG_BACNET_BASIC_OBJECT_ANALOG_INPUT
#define CONFIG_BACNET_BASIC_OBJECT_BINARY_INPUT

#endif /* BACNET_CONFIG_H_PORT */
