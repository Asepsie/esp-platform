/**
 * @file bacnet_transport.h
 * @brief BACnet transport abstraction — pluggable datalinks (SC, MS/TP, ...).
 *
 * The BACnet server speaks the same object model over one or more transports
 * registered at startup. Each transport implements a small ops vtable; the
 * server can run BACnet/SC and MS/TP simultaneously (same objects visible on
 * both). This header defines the address type, the ops vtable, and the
 * registration entry point.
 */
#ifndef BACNET_TRANSPORT_H
#define BACNET_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>
#include "platform_compat.h"   /* esp_err_t */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief A BACnet datalink address.
 *
 * Generic enough for MS/TP (1-byte MAC) and SC/IP (6+ byte MAC). @c net is the
 * BACnet network number (0 = local).
 */
typedef struct {
    uint16_t net;        /**< BACnet network number (0 = local).        */
    uint8_t  mac_len;    /**< Valid bytes in @c mac (1 = MS/TP).         */
    uint8_t  mac[7];     /**< MAC address bytes.                         */
} bacnet_addr_t;

/**
 * @brief Transport (datalink) operations vtable.
 *
 * All function pointers are required (non-NULL) except @c cleanup.
 */
typedef struct {
    /** Bring the datalink up. */
    esp_err_t (*init)(void);
    /** Send one BACnet PDU to @p dest. */
    esp_err_t (*send)(const uint8_t *pdu, size_t len, bacnet_addr_t *dest);
    /** Receive one PDU (up to @p max_len) within @p timeout_ms; fills @p src. */
    esp_err_t (*receive)(uint8_t *pdu, size_t max_len, size_t *len,
                         bacnet_addr_t *src, uint32_t timeout_ms);
    /** Tear the datalink down (optional, may be NULL). */
    void (*cleanup)(void);
} bacnet_transport_ops_t;

/**
 * @brief Register a transport with the BACnet server.
 *
 * Calls the transport's @c init() and, on success, adds it to the active set.
 * Multiple transports (e.g. SC + MS/TP) may be registered.
 *
 * @param transport Non-NULL ops vtable with non-NULL init/send/receive.
 * @retval ESP_OK              Registered.
 * @retval ESP_ERR_INVALID_ARG NULL vtable or missing required op.
 * @retval ESP_ERR_NO_MEM      Transport table full.
 * @return Otherwise the @c esp_err_t from the transport's init().
 */
esp_err_t bacnet_server_add_transport(const bacnet_transport_ops_t *transport);

/** @brief Number of transports currently registered (diagnostic). */
size_t bacnet_server_transport_count(void);

/**
 * @brief Registered transport ops by index, or NULL if @p index is out of range.
 *
 * Lets the datalink glue fan a PDU out to / poll every active datalink without
 * owning the registry. Indices are stable for the life of the registration set.
 */
const bacnet_transport_ops_t *bacnet_transport_get(size_t index);

/** @brief MS/TP transport ops (stub datalink over hal_uart_mstp). */
const bacnet_transport_ops_t *bacnet_transport_mstp_ops(void);

#ifdef __cplusplus
}
#endif

#endif /* BACNET_TRANSPORT_H */
