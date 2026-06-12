/**
 * @file bacnet_server.h
 * @brief BACnet server lifecycle — the M0 device (discoverable, readable).
 *
 * Brings up the vendored bacnet-stack as a BACnet device: the Device object,
 * AI/BI objects instantiated from bacnet_object_map, the standard service
 * handlers (Who-Is/I-Am, ReadProperty, ReadPropertyMultiple), and a polling
 * task that services the datalink, runs stack timers, and refreshes present
 * values from sensor_state. Datalinks are registered separately via
 * bacnet_server_add_transport() (see bacnet_transport.h).
 */
#ifndef BACNET_SERVER_H
#define BACNET_SERVER_H

#include "platform_compat.h" /* esp_err_t */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the BACnet device (objects + service handlers).
 *
 * Does NOT start the task and does NOT register transports — call
 * bacnet_server_add_transport() before bacnet_server_start() so the device can
 * answer on the wire. Safe to call once at boot.
 *
 * @retval ESP_OK Stack and objects initialized.
 */
esp_err_t bacnet_server_init(void);

/**
 * @brief Start the bacnet_task (datalink service, timers, PV refresh, I-Am).
 *
 * @retval ESP_OK         Task created.
 * @retval ESP_ERR_NO_MEM Task could not be created.
 */
esp_err_t bacnet_server_start(void);

#ifdef __cplusplus
}
#endif

#endif /* BACNET_SERVER_H */
