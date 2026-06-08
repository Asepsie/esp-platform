// =============================================================================
// test_cluster_map.c — host unit tests for the Layer 1→2 cluster map.
// =============================================================================
#include "unity.h"
#include "cluster_map.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

// Known temperature mapping resolves with the expected semantics + bounds.
static void test_lookup_temperature(void)
{
    const cluster_map_entry_t *e = cluster_map_lookup(0x0402, 0x0000);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_INT(DATA_CAT_TEMPERATURE, e->category);
    TEST_ASSERT_EQUAL_STRING("Zone-Temperature", e->bacnet_object_name);
    TEST_ASSERT_EQUAL_FLOAT(0.01f, e->scale_factor);
    TEST_ASSERT_EQUAL_FLOAT(-40.0f, e->min_plausible);
    TEST_ASSERT_EQUAL_FLOAT(80.0f, e->max_plausible);
}

// Each documented row is present with the right category.
static void test_lookup_all_known(void)
{
    TEST_ASSERT_EQUAL_INT(DATA_CAT_HUMIDITY,    cluster_map_category(0x0405, 0x0000));
    TEST_ASSERT_EQUAL_INT(DATA_CAT_CO2,         cluster_map_category(0x040D, 0x0000));
    TEST_ASSERT_EQUAL_INT(DATA_CAT_DRY_CONTACT, cluster_map_category(0x000F, 0x0055));
    TEST_ASSERT_EQUAL_INT(DATA_CAT_BATTERY,     cluster_map_category(0x0001, 0x0021));
}

// Battery is internal-only: no northbound BACnet name.
static void test_battery_not_northbound(void)
{
    const cluster_map_entry_t *e = cluster_map_lookup(0x0001, 0x0021);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_NULL(e->bacnet_object_name);
}

// Unmapped pairs return NULL / UNKNOWN.
static void test_lookup_unknown(void)
{
    TEST_ASSERT_NULL(cluster_map_lookup(0xFFFF, 0x0000));
    // Right cluster, wrong attribute is still unmapped.
    TEST_ASSERT_NULL(cluster_map_lookup(0x0402, 0x1234));
    TEST_ASSERT_EQUAL_INT(DATA_CAT_UNKNOWN, cluster_map_category(0xABCD, 0xEF01));
}

// Table accessors are consistent.
static void test_table_iteration(void)
{
    TEST_ASSERT_EQUAL_size_t(5, cluster_map_size());
    TEST_ASSERT_NULL(cluster_map_get(cluster_map_size())); // out of range
    for (size_t i = 0; i < cluster_map_size(); i++) {
        const cluster_map_entry_t *e = cluster_map_get(i);
        TEST_ASSERT_NOT_NULL(e);
        // Every row must be self-consistent: lookup finds the same entry.
        TEST_ASSERT_EQUAL_PTR(e, cluster_map_lookup(e->cluster_id, e->attribute_id));
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_lookup_temperature);
    RUN_TEST(test_lookup_all_known);
    RUN_TEST(test_battery_not_northbound);
    RUN_TEST(test_lookup_unknown);
    RUN_TEST(test_table_iteration);
    return UNITY_END();
}
