// =============================================================================
// test_uart_bridge_framing.c — host unit tests for the UART bridge framing.
//
// Covers the framing contract that BOTH firmwares depend on:
//   * encode a SENSOR_REPORT, decode it back, verify every field round-trips
//   * a frame with a corrupted CRC is rejected
//   * a truncated frame is rejected
//   * a frame with the wrong SOF byte is rejected
//
// Pure host test: links against ../../components/uart_bridge/uart_bridge_framing.c.
// =============================================================================
#include "unity.h"
#include "uart_bridge_protocol.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

// Build a representative SENSOR_REPORT payload (e.g. an SNZB-02P temp report).
static bridge_sensor_report_t make_sample_report(void)
{
    bridge_sensor_report_t r;
    memset(&r, 0, sizeof(r)); // zero padding/unused so comparisons are stable
    const uint8_t ieee[8] = {0x00, 0x12, 0x4B, 0x00, 0x21, 0x5A, 0x3C, 0x7D};
    memcpy(r.ieee_addr, ieee, sizeof(ieee));
    r.cluster_id   = 0x0402; // ZCL temperature measurement
    r.attribute_id = 0x0000; // MeasuredValue
    r.data_type    = 0x29;   // int16
    r.value_float  = 21.5f;
    r.value_bool   = false;
    r.lqi          = 200;
    r.battery_pct  = 95;
    return r;
}

// encode a SENSOR_REPORT, decode it, verify fields match
static void test_encode_decode_sensor_report_roundtrip(void)
{
    const bridge_sensor_report_t in = make_sample_report();

    uint8_t frame[BRIDGE_MAX_FRAME];
    const int frame_len = bridge_frame_encode(MSG_SENSOR_REPORT,
                                              (const uint8_t *)&in, sizeof(in),
                                              frame, sizeof(frame));

    // Frame length = overhead + payload, and header bytes are as specified.
    TEST_ASSERT_EQUAL_INT((int)(BRIDGE_FRAME_OVERHEAD + sizeof(in)), frame_len);
    TEST_ASSERT_EQUAL_HEX8(BRIDGE_SOF, frame[BRIDGE_OFF_SOF]);
    TEST_ASSERT_EQUAL_HEX8(MSG_SENSOR_REPORT, frame[BRIDGE_OFF_TYPE]);
    TEST_ASSERT_EQUAL_UINT16(sizeof(in),
        (uint16_t)frame[BRIDGE_OFF_LEN] | ((uint16_t)frame[BRIDGE_OFF_LEN + 1] << 8));

    bridge_msg_type_t type = 0;
    bridge_sensor_report_t out;
    uint16_t out_len = 0;
    const bridge_decode_status_t st = bridge_frame_decode(
        frame, (size_t)frame_len, &type, (uint8_t *)&out, sizeof(out), &out_len);

    TEST_ASSERT_EQUAL_INT(BRIDGE_DECODE_OK, st);
    TEST_ASSERT_EQUAL_HEX8(MSG_SENSOR_REPORT, type);
    TEST_ASSERT_EQUAL_UINT16(sizeof(in), out_len);

    // Every field survives the round-trip.
    TEST_ASSERT_EQUAL_UINT8_ARRAY(in.ieee_addr, out.ieee_addr, 8);
    TEST_ASSERT_EQUAL_HEX16(in.cluster_id, out.cluster_id);
    TEST_ASSERT_EQUAL_HEX16(in.attribute_id, out.attribute_id);
    TEST_ASSERT_EQUAL_HEX8(in.data_type, out.data_type);
    TEST_ASSERT_EQUAL_FLOAT(in.value_float, out.value_float);
    TEST_ASSERT_EQUAL(in.value_bool, out.value_bool);
    TEST_ASSERT_EQUAL_UINT8(in.lqi, out.lqi);
    TEST_ASSERT_EQUAL_UINT8(in.battery_pct, out.battery_pct);
}

// bad CRC rejected
static void test_decode_bad_crc_rejected(void)
{
    const bridge_sensor_report_t in = make_sample_report();
    uint8_t frame[BRIDGE_MAX_FRAME];
    const int frame_len = bridge_frame_encode(MSG_SENSOR_REPORT,
                                              (const uint8_t *)&in, sizeof(in),
                                              frame, sizeof(frame));
    TEST_ASSERT_GREATER_THAN_INT(0, frame_len);

    // Flip a payload bit so the stored CRC no longer matches.
    frame[BRIDGE_OFF_PAYLOAD] ^= 0x01;

    const bridge_decode_status_t st = bridge_frame_decode(
        frame, (size_t)frame_len, NULL, NULL, 0, NULL);
    TEST_ASSERT_EQUAL_INT(BRIDGE_DECODE_ERR_BAD_CRC, st);
}

// truncated frame rejected
static void test_decode_truncated_frame_rejected(void)
{
    const bridge_sensor_report_t in = make_sample_report();
    uint8_t frame[BRIDGE_MAX_FRAME];
    const int frame_len = bridge_frame_encode(MSG_SENSOR_REPORT,
                                              (const uint8_t *)&in, sizeof(in),
                                              frame, sizeof(frame));
    TEST_ASSERT_GREATER_THAN_INT(0, frame_len);

    // Present one byte fewer than the LEN field promises.
    const bridge_decode_status_t st = bridge_frame_decode(
        frame, (size_t)frame_len - 1, NULL, NULL, 0, NULL);
    TEST_ASSERT_EQUAL_INT(BRIDGE_DECODE_ERR_TOO_SHORT, st);
}

// wrong SOF rejected
static void test_decode_wrong_sof_rejected(void)
{
    const bridge_sensor_report_t in = make_sample_report();
    uint8_t frame[BRIDGE_MAX_FRAME];
    const int frame_len = bridge_frame_encode(MSG_SENSOR_REPORT,
                                              (const uint8_t *)&in, sizeof(in),
                                              frame, sizeof(frame));
    TEST_ASSERT_GREATER_THAN_INT(0, frame_len);

    // Corrupt the start-of-frame marker; full length, so SOF is what fails.
    frame[BRIDGE_OFF_SOF] = 0x55;

    const bridge_decode_status_t st = bridge_frame_decode(
        frame, (size_t)frame_len, NULL, NULL, 0, NULL);
    TEST_ASSERT_EQUAL_INT(BRIDGE_DECODE_ERR_BAD_SOF, st);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_encode_decode_sensor_report_roundtrip);
    RUN_TEST(test_decode_bad_crc_rejected);
    RUN_TEST(test_decode_truncated_frame_rejected);
    RUN_TEST(test_decode_wrong_sof_rejected);
    return UNITY_END();
}
