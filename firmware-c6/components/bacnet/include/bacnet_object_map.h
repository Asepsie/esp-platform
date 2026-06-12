/**
 * @file bacnet_object_map.h
 * @brief The static BACnet object table — the single source of "what objects
 *        this device exposes" (data-model Layer 5).
 *
 * Mirrors the cluster_map pattern: one immutable table, one row per object, pure
 * C, zero dependencies beyond data_model.h — so it builds identically on target
 * and in host unit tests. The table is the design intent; bacnet_server.c
 * instantiates from it, and bacnet_objects.c resolves each row's present value
 * from sensor_state.
 *
 * Instance allocation follows CLAUDE.md:
 *   0–99    space-aggregated values        (AI/BI, read-only)
 *   100–199 per-equipment raw values       (AI/BI, read-only)
 *   200–299 setpoints / modes / relays     (AV/BV/MSV/BO, writable — M2)
 *   300–399 diagnostics                     (AI/BV, read-only)
 */
#ifndef BACNET_OBJECT_MAP_H
#define BACNET_OBJECT_MAP_H

#include <stddef.h>
#include "data_model.h" /* bacnet_object_map_entry_t, bacnet_obj_type_t */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Number of rows in the object table. */
size_t bacnet_object_map_size(void);

/** @brief Row by index, or NULL if @p index is out of range. */
const bacnet_object_map_entry_t *bacnet_object_map_get(size_t index);

/** @brief Row whose instance == @p instance, or NULL if none. */
const bacnet_object_map_entry_t *bacnet_object_map_lookup(uint32_t instance);

#ifdef __cplusplus
}
#endif

#endif /* BACNET_OBJECT_MAP_H */
