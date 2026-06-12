/**
 * @file bacnet_port_systime.c
 * @brief Millisecond time source for the vendored bacnet-stack (port hook).
 *
 * bacnet-stack's basic/sys/mstimer.c keeps all timer math platform-independent
 * and expects the platform to provide a monotonic millisecond clock via
 * mstimer_now(). We back it with esp_timer (brought up at boot). This is a port
 * file — it is the platform's, not the stack's, so it lives outside vendor/.
 */
#include "esp_timer.h"

unsigned long mstimer_now(void)
{
    return (unsigned long)(esp_timer_get_time() / 1000ULL);
}
