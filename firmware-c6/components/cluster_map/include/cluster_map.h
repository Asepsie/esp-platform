// =============================================================================
// cluster_map.h — Layer 1→2 bridge: ZCL (cluster, attribute) → semantics.
//
// Single source of truth for cluster-to-semantic mappings (data-model §2.3).
// Add a new sensor type by adding one row to CLUSTER_MAP in cluster_map.c —
// nothing else changes. Pure C (no ESP-IDF), so it is host-unit-tested.
// =============================================================================
#ifndef CLUSTER_MAP_H
#define CLUSTER_MAP_H

#include <stddef.h>
#include "data_model.h"

#ifdef __cplusplus
extern "C" {
#endif

// Look up the map entry for a (cluster, attribute) pair.
// Returns NULL if the pair is not mapped.
const cluster_map_entry_t *cluster_map_lookup(uint16_t cluster_id,
                                              uint16_t attribute_id);

// Convenience: semantic category for a pair, or DATA_CAT_UNKNOWN if unmapped.
data_category_t cluster_map_category(uint16_t cluster_id, uint16_t attribute_id);

// Number of rows in the cluster map.
size_t cluster_map_size(void);

// Indexed access to the table (for iteration/diagnostics).
// Returns NULL if index is out of range.
const cluster_map_entry_t *cluster_map_get(size_t index);

#ifdef __cplusplus
}
#endif

#endif // CLUSTER_MAP_H
