// =============================================================================
// zigbee_cluster_handler.h — ZCL attribute report -> bridge sensor report.
//
// The H2 equivalent of the C6's cluster_map (data-model §2.3): it turns a raw
// ZCL attribute (cluster, attribute, data-type, raw bytes) received from a
// paired Zigbee device into a wire-ready bridge_sensor_report_t for the UART
// bridge to send to the C6.
//
// PURE C — no esp-zigbee-sdk, no radio. Depends only on the shared protocol
// header and platform_compat (esp_err_t). This is the host-testable half of the
// coordinator; the SDK-calling half lives in zigbee_coordinator.c (target only).
// =============================================================================
#ifndef ZIGBEE_CLUSTER_HANDLER_H
#define ZIGBEE_CLUSTER_HANDLER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "platform_compat.h"      // esp_err_t / ESP_OK / ESP_ERR_* (host shim or esp_err.h)
#include "uart_bridge_protocol.h" // bridge_sensor_report_t

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Convert a raw ZCL attribute report into a bridge sensor report ready
 *        for UART transmission to the C6.
 *
 * Handles ZCL data-type decoding, unit conversion (ZCL raw -> engineering
 * units), and plausibility checking. Pure C — no Zigbee SDK dependency.
 * Host-testable. The (cluster, attribute) -> scale/range table mirrors the C6
 * cluster_map; per the data model the H2 is the producer that applies scaling.
 *
 * On ESP_OK @p report is fully populated (ieee/cluster/attribute/data_type,
 * the value as value_float and/or value_bool, lqi, and battery_pct which is
 * 0xFF "unknown" unless this report is itself a battery attribute). On any
 * error @p report is left untouched.
 *
 * @param[in]  src_ieee   Source device IEEE address (8 bytes, little-endian)
 * @param[in]  cluster_id ZCL cluster ID
 * @param[in]  attr_id    ZCL attribute ID
 * @param[in]  data_type  ZCL data type tag (e.g. 0x29 int16, 0x21 uint16, 0x39 single)
 * @param[in]  raw_data   Raw attribute value bytes (little-endian, as on the wire)
 * @param[in]  raw_len    Length of @p raw_data in bytes
 * @param[in]  lqi        Link quality indicator 0-255
 * @param[out] report     Filled bridge_sensor_report_t on ESP_OK
 *
 * @return ESP_OK              Report filled successfully
 * @return ESP_ERR_NOT_FOUND   Unknown cluster/attribute combo
 * @return ESP_ERR_INVALID_ARG NULL/short args, undecodable data type, or value
 *                             outside plausibility bounds (rejected, not filled)
 */
esp_err_t zigbee_cluster_handler_process(const uint8_t *src_ieee,
                                         uint16_t cluster_id,
                                         uint16_t attr_id,
                                         uint8_t data_type,
                                         const uint8_t *raw_data,
                                         size_t raw_len,
                                         uint8_t lqi,
                                         bridge_sensor_report_t *report);

/**
 * @brief Check if a cluster/attribute pair is one we care about.
 *
 * Lets the coordinator cheaply skip attributes it has no mapping for before
 * attempting a full conversion. Pure lookup, no side effects.
 */
bool zigbee_cluster_handler_is_supported(uint16_t cluster_id, uint16_t attr_id);

#ifdef __cplusplus
}
#endif

#endif // ZIGBEE_CLUSTER_HANDLER_H
