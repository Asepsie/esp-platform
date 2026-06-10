/**
 * @file bacnet_transport.c
 * @brief BACnet transport registry.
 *
 * Holds the set of active datalinks. The full server (object model, APDU
 * handling, COV) is future work; this just wires transport registration so SC
 * and MS/TP can coexist. No driver headers here — transports go through HALs.
 */
#include "bacnet_transport.h"

#define BACNET_MAX_TRANSPORTS 2  // e.g. SC + MS/TP simultaneously

static const bacnet_transport_ops_t *s_transports[BACNET_MAX_TRANSPORTS];
static size_t s_count;

esp_err_t bacnet_server_add_transport(const bacnet_transport_ops_t *transport)
{
    if (transport == NULL || transport->init == NULL ||
        transport->send == NULL || transport->receive == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_count >= BACNET_MAX_TRANSPORTS) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = transport->init();
    if (err != ESP_OK) {
        return err;
    }
    s_transports[s_count++] = transport;
    return ESP_OK;
}

size_t bacnet_server_transport_count(void)
{
    return s_count;
}
