/**
 * @file bacnet_objects.h
 * @brief Present-value binding: BACnet object map row → sensor_state value.
 *
 * The thin adapter that projects sensor_state (the single source of truth,
 * RT-04) onto BACnet present values. Pure logic over the sensor_state API and
 * the object map — no bacnet-stack dependency — so it is host-testable against
 * the real sensor_state (or its mock). bacnet_server.c calls this each refresh
 * cycle and pushes the result into the stack's AI/BI objects.
 */
#ifndef BACNET_OBJECTS_H
#define BACNET_OBJECTS_H

#include "platform_compat.h"      /* esp_err_t */
#include "bacnet_object_map.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Resolve the present value for one object-map row from sensor_state.
 *
 * Resolution order:
 *   1. Diagnostic / store-owned instances → sensor_state_get_bacnet_value().
 *   2. Space-aggregated rows (source_space_id set) → the matching field of the
 *      space's aggregated data, selected by source_category.
 *
 * @param e   Object map row (non-NULL).
 * @param out Receives the present value (engineering units; 0/1 for binary).
 * @retval ESP_OK              Value resolved.
 * @retval ESP_ERR_INVALID_ARG NULL argument or unsupported category.
 * @retval ESP_ERR_NOT_FOUND   No backing value yet (e.g. topology not loaded).
 * @return Otherwise the esp_err_t from the sensor_state call.
 */
esp_err_t bacnet_object_present_value(const bacnet_object_map_entry_t *e,
                                      float *out);

#ifdef __cplusplus
}
#endif

#endif /* BACNET_OBJECTS_H */
