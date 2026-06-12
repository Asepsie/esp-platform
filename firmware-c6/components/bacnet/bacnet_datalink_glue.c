// =============================================================================
// bacnet_datalink_glue.c — bacnet-stack datalink_*() over bacnet_transport.
//
// We do NOT compile the stack's datalink.c dispatcher (no BACDL_* is defined).
// Instead this file provides the datalink_*() entry points the stack calls and
// fans them out to every registered bacnet_transport (design §7 option A:
// one multi-homed device, same objects on every datalink). The PDU handed to
// datalink_send_pdu() is already a full NPDU+APDU, so — like the stack's own
// BACnet/IP datalink — we ignore npdu_data and ship the bytes as-is. Real
// MS/TP framing/token passing is M4, inside the MS/TP transport itself.
// =============================================================================
#include <string.h>

#include "bacnet/bacdef.h"
#include "bacnet/npdu.h"
#include "bacnet/datalink/datalink.h"

#include "bacnet_transport.h"

// --- address mapping: bacnet-stack BACNET_ADDRESS <-> our bacnet_addr_t -------
static void to_transport_addr(const BACNET_ADDRESS *src, bacnet_addr_t *dst)
{
    memset(dst, 0, sizeof(*dst));
    dst->net = src->net;
    dst->mac_len = src->mac_len;
    if (dst->mac_len > sizeof(dst->mac)) {
        dst->mac_len = sizeof(dst->mac);
    }
    memcpy(dst->mac, src->mac, dst->mac_len);
}

static void from_transport_addr(const bacnet_addr_t *src, BACNET_ADDRESS *dst)
{
    memset(dst, 0, sizeof(*dst));
    dst->net = src->net;
    dst->mac_len = src->mac_len;
    if (dst->mac_len > MAX_MAC_LEN) {
        dst->mac_len = MAX_MAC_LEN;
    }
    memcpy(dst->mac, src->mac, dst->mac_len);
}

// --- datalink_*(): the surface bacnet-stack links against ---------------------

bool datalink_init(char *ifname)
{
    (void)ifname; // transports are brought up by bacnet_server_add_transport()
    return true;
}

int datalink_send_pdu(BACNET_ADDRESS *dest, BACNET_NPDU_DATA *npdu_data,
                      uint8_t *pdu, unsigned pdu_len)
{
    (void)npdu_data; // NPDU header already encoded into pdu by the caller
    if (pdu == NULL || pdu_len == 0) {
        return 0;
    }
    bacnet_addr_t d;
    to_transport_addr(dest, &d);

    // mac_len == 0 (or broadcast network) means broadcast: send on every link.
    // A unicast still goes to every link; the addressed datalink delivers it,
    // the others drop an unknown MAC (acceptable for a multi-homed device).
    size_t n = bacnet_server_transport_count();
    for (size_t i = 0; i < n; i++) {
        const bacnet_transport_ops_t *t = bacnet_transport_get(i);
        if (t != NULL) {
            (void)t->send(pdu, pdu_len, &d);
        }
    }
    return (int)pdu_len;
}

uint16_t datalink_receive(BACNET_ADDRESS *src, uint8_t *pdu, uint16_t max_pdu,
                          unsigned timeout)
{
    size_t n = bacnet_server_transport_count();
    for (size_t i = 0; i < n; i++) {
        const bacnet_transport_ops_t *t = bacnet_transport_get(i);
        if (t == NULL) {
            continue;
        }
        size_t len = 0;
        bacnet_addr_t s;
        esp_err_t err = t->receive(pdu, max_pdu, &len, &s, timeout);
        if (err == ESP_OK && len > 0) {
            from_transport_addr(&s, src);
            return (uint16_t)len;
        }
    }
    return 0;
}

void datalink_cleanup(void) {}

void datalink_get_broadcast_address(BACNET_ADDRESS *dest)
{
    if (dest != NULL) {
        memset(dest, 0, sizeof(*dest));
        dest->mac_len = 0; // mac_len 0 == broadcast
        dest->net = BACNET_BROADCAST_NETWORK;
    }
}

void datalink_get_my_address(BACNET_ADDRESS *my_address)
{
    if (my_address != NULL) {
        memset(my_address, 0, sizeof(*my_address));
        // Local station: the active transport's own MAC is filled per-datalink
        // once real framing lands (M4). Net 0 = local.
    }
}

void datalink_set_interface(char *ifname)
{
    (void)ifname;
}

void datalink_set(char *datalink_string)
{
    (void)datalink_string;
}

void datalink_maintenance_timer(uint16_t seconds)
{
    (void)seconds; // no periodic datalink maintenance in the glue (M4: MS/TP)
}
