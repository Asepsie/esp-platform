// =============================================================================
// uart_bridge_protocol.h — C6 <-> H2 UART bridge: shared types + frame format.
//
// THIS FILE IS THE WIRE CONTRACT between firmware-c6 and firmware-h2 and MUST
// be byte-identical in both projects. Any change requires a simultaneous PR to
// both. See docs/architecture/uart-bridge-protocol.md (the authoritative spec).
//
// Frame format (section 3 of the spec):
//
//   ┌────────┬──────────┬──────────┬──────────────┬─────────┐
//   │ SOF    │ MSG_TYPE │ LEN      │ PAYLOAD      │ CRC16   │
//   │ 1 byte │ 1 byte   │ 2 bytes  │ 0–256 bytes  │ 2 bytes │
//   │ 0xAA   │          │ LE u16   │              │ CCITT   │
//   └────────┴──────────┴──────────┴──────────────┴─────────┘
//
//   - LEN is the payload length only, little-endian uint16.
//   - CRC16 covers MSG_TYPE + LEN + PAYLOAD (i.e. everything except SOF and the
//     CRC field itself) and is transmitted little-endian.
//   - CRC variant: CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection,
//     xorout 0x0000). The spec says "CCITT polynomial 0x1021"; this is the
//     concrete, agreed parameterization — both sides MUST use exactly this.
//
// Endianness note: ESP32-C6 and ESP32-H2 are both little-endian, and host unit
// tests run on little-endian x86. Multi-byte payload struct fields are therefore
// sent as their in-memory bytes (no per-field byte swapping). The framing code
// (LEN, CRC) does its own explicit little-endian byte handling so it is correct
// regardless of host endianness.
// =============================================================================
#ifndef UART_BRIDGE_PROTOCOL_H
#define UART_BRIDGE_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- Frame constants ---------------------------------------------------------
#define BRIDGE_SOF              0xAAu  // start-of-frame marker
#define BRIDGE_MAX_PAYLOAD      256u   // max payload bytes (spec section 3)
// Non-payload bytes: SOF(1) + MSG_TYPE(1) + LEN(2) + CRC16(2).
#define BRIDGE_FRAME_OVERHEAD   6u
// Largest possible full frame.
#define BRIDGE_MAX_FRAME        (BRIDGE_FRAME_OVERHEAD + BRIDGE_MAX_PAYLOAD)

// Byte offsets within a frame (payload starts at BRIDGE_OFF_PAYLOAD).
#define BRIDGE_OFF_SOF          0u
#define BRIDGE_OFF_TYPE         1u
#define BRIDGE_OFF_LEN          2u
#define BRIDGE_OFF_PAYLOAD      4u

// --- Message types (spec section 4) ------------------------------------------
// Single enum spans both directions; values are globally unique.
typedef enum {
    // H2 -> C6 (sensor data direction)
    MSG_SENSOR_REPORT = 0x01,  // bridge_sensor_report_t
    MSG_DEVICE_JOIN   = 0x02,  // bridge_device_join_t
    MSG_DEVICE_LEAVE  = 0x03,  // ieee[8]
    MSG_DEVICE_STATUS = 0x04,  // ieee[8] + online[1]

    // C6 -> H2 (command direction)
    MSG_PERMIT_JOIN   = 0x10,  // duration_s[1]
    MSG_POLL_ATTR     = 0x11,  // ieee[8] + cluster[2] + attr[2]
    MSG_BIND_DEVICE   = 0x12,  // ieee[8] + space_id[16]
    MSG_REMOVE_DEVICE = 0x13,  // ieee[8]
    MSG_OTA_START     = 0x20,  // image_size[4] + sha256[32]
    MSG_OTA_DATA      = 0x21,  // chunk_seq[2] + data[0..128]
    MSG_OTA_END       = 0x22,  // (no payload)

    // Bidirectional
    MSG_HEARTBEAT     = 0x30,  // sequence[4], every 5 s
    MSG_NACK          = 0x31,  // orig_type[1] + reason[1]
} bridge_msg_type_t;

// --- Payload structs (spec sections 4.1, 4.2) --------------------------------
// Sent as raw packed bytes; see endianness note above.

typedef struct __attribute__((packed)) {
    uint8_t  ieee_addr[8];   // Zigbee IEEE address (little-endian)
    uint16_t cluster_id;     // ZCL cluster (e.g. 0x0402 = temperature)
    uint16_t attribute_id;   // ZCL attribute
    uint8_t  data_type;      // ZCL data type tag
    float    value_float;    // normalized value (°C, %, ppm)
    bool     value_bool;     // for binary attributes
    uint8_t  lqi;            // link quality 0–255
    uint8_t  battery_pct;    // battery % (0xFF = unknown)
} bridge_sensor_report_t;    // 20 bytes
_Static_assert(sizeof(bridge_sensor_report_t) == 20,
               "bridge_sensor_report_t must be 20 bytes (wire layout)");

typedef struct __attribute__((packed)) {
    uint8_t  ieee_addr[8];
    uint16_t short_addr;
    uint16_t supported_clusters[8];
    uint8_t  cluster_count;
    char     manufacturer[32];
    char     model[32];
} bridge_device_join_t;
// NOTE: with 16-bit cluster IDs this is 91 bytes, not the "83" stated in the
// spec (the spec appears to have counted supported_clusters[8] as 8 bytes
// instead of 16). 91 still fits the 256-byte max payload. Flag to spec owner.

// --- Framing API (implemented in uart_bridge_framing.c) ----------------------

// CRC-16/CCITT-FALSE over `len` bytes of `data`. See header notes for params.
uint16_t bridge_crc16(const uint8_t *data, size_t len);

// Encode a frame for `type` carrying `payload` (`payload_len` bytes, may be 0)
// into `out` (capacity `out_cap`). Returns the total frame length written, or
// -1 on bad args / payload too large / insufficient capacity.
int bridge_frame_encode(bridge_msg_type_t type,
                        const uint8_t *payload, uint16_t payload_len,
                        uint8_t *out, size_t out_cap);

// Result of decoding a single, complete frame buffer.
typedef enum {
    BRIDGE_DECODE_OK = 0,
    BRIDGE_DECODE_ERR_NULL,        // null frame pointer
    BRIDGE_DECODE_ERR_TOO_SHORT,   // buffer smaller than the frame requires (truncated)
    BRIDGE_DECODE_ERR_BAD_SOF,     // first byte is not BRIDGE_SOF
    BRIDGE_DECODE_ERR_BAD_LEN,     // declared length invalid, or output buffer too small
    BRIDGE_DECODE_ERR_BAD_CRC,     // CRC mismatch
} bridge_decode_status_t;

// Decode one complete frame from `frame` (`frame_len` bytes available). On
// BRIDGE_DECODE_OK, writes the message type, copies the payload into
// `payload_out` (if non-NULL, capacity `payload_cap`), and reports the payload
// length. Output params may be NULL if not needed.
bridge_decode_status_t bridge_frame_decode(const uint8_t *frame, size_t frame_len,
                                           bridge_msg_type_t *type_out,
                                           uint8_t *payload_out, size_t payload_cap,
                                           uint16_t *payload_len_out);

#ifdef __cplusplus
}
#endif

#endif // UART_BRIDGE_PROTOCOL_H
