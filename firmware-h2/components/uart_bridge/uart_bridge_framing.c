// =============================================================================
// uart_bridge_framing.c — encode/decode/CRC for the C6 <-> H2 UART bridge.
//
// Pure C, no ESP-IDF and no hardware dependencies, so it builds and runs in
// host unit tests (see firmware-h2/tests/host/test_uart_bridge_framing.c).
// The frame format and CRC parameters are defined in uart_bridge_protocol.h.
// =============================================================================
#include "uart_bridge_protocol.h"

#include <string.h>

// CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no input/output reflection,
// xorout 0x0000. Bit-by-bit (table-free) — these frames are tiny and infrequent,
// so the simple form is plenty fast and keeps the wire contract obvious.
uint16_t bridge_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x8000u) {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

int bridge_frame_encode(bridge_msg_type_t type,
                        const uint8_t *payload, uint16_t payload_len,
                        uint8_t *out, size_t out_cap)
{
    if (out == NULL) {
        return -1;
    }
    if (payload_len > BRIDGE_MAX_PAYLOAD) {
        return -1;
    }
    if (payload_len > 0 && payload == NULL) {
        return -1;
    }

    const size_t total = (size_t)BRIDGE_FRAME_OVERHEAD + payload_len;
    if (out_cap < total) {
        return -1;
    }

    out[BRIDGE_OFF_SOF]      = BRIDGE_SOF;
    out[BRIDGE_OFF_TYPE]     = (uint8_t)type;
    out[BRIDGE_OFF_LEN]      = (uint8_t)(payload_len & 0xFFu);        // LE low
    out[BRIDGE_OFF_LEN + 1]  = (uint8_t)((payload_len >> 8) & 0xFFu); // LE high

    if (payload_len > 0) {
        memcpy(&out[BRIDGE_OFF_PAYLOAD], payload, payload_len);
    }

    // CRC covers MSG_TYPE + LEN + PAYLOAD: bytes [1 .. 3+payload_len].
    const uint16_t crc = bridge_crc16(&out[BRIDGE_OFF_TYPE], (size_t)3 + payload_len);
    const size_t crc_off = (size_t)BRIDGE_OFF_PAYLOAD + payload_len;
    out[crc_off]     = (uint8_t)(crc & 0xFFu);        // LE low
    out[crc_off + 1] = (uint8_t)((crc >> 8) & 0xFFu); // LE high

    return (int)total;
}

bridge_decode_status_t bridge_frame_decode(const uint8_t *frame, size_t frame_len,
                                           bridge_msg_type_t *type_out,
                                           uint8_t *payload_out, size_t payload_cap,
                                           uint16_t *payload_len_out)
{
    if (frame == NULL) {
        return BRIDGE_DECODE_ERR_NULL;
    }
    // Need at least an empty frame (SOF + TYPE + LEN + CRC).
    if (frame_len < BRIDGE_FRAME_OVERHEAD) {
        return BRIDGE_DECODE_ERR_TOO_SHORT;
    }
    if (frame[BRIDGE_OFF_SOF] != BRIDGE_SOF) {
        return BRIDGE_DECODE_ERR_BAD_SOF;
    }

    const uint16_t payload_len = (uint16_t)frame[BRIDGE_OFF_LEN]
                               | ((uint16_t)frame[BRIDGE_OFF_LEN + 1] << 8);
    if (payload_len > BRIDGE_MAX_PAYLOAD) {
        return BRIDGE_DECODE_ERR_BAD_LEN;
    }

    const size_t expected = (size_t)BRIDGE_FRAME_OVERHEAD + payload_len;
    if (frame_len < expected) {
        return BRIDGE_DECODE_ERR_TOO_SHORT; // truncated mid-payload/CRC
    }

    const uint16_t crc_calc = bridge_crc16(&frame[BRIDGE_OFF_TYPE], (size_t)3 + payload_len);
    const size_t crc_off = (size_t)BRIDGE_OFF_PAYLOAD + payload_len;
    const uint16_t crc_rx = (uint16_t)frame[crc_off]
                          | ((uint16_t)frame[crc_off + 1] << 8);
    if (crc_calc != crc_rx) {
        return BRIDGE_DECODE_ERR_BAD_CRC;
    }

    if (payload_out != NULL) {
        if (payload_cap < payload_len) {
            return BRIDGE_DECODE_ERR_BAD_LEN; // caller's buffer too small
        }
        if (payload_len > 0) {
            memcpy(payload_out, &frame[BRIDGE_OFF_PAYLOAD], payload_len);
        }
    }
    if (type_out != NULL) {
        *type_out = (bridge_msg_type_t)frame[BRIDGE_OFF_TYPE];
    }
    if (payload_len_out != NULL) {
        *payload_len_out = payload_len;
    }
    return BRIDGE_DECODE_OK;
}
