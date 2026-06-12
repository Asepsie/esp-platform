// =============================================================================
// test_bacnet_object_map.c — host unit tests for the BACnet object table.
//
// Pure table integrity (no stack, no sensor_state): unique instances, valid
// instance ranges per CLAUDE.md, type/category consistency, COV sanity.
// =============================================================================
#include "unity.h"
#include "bacnet_object_map.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

// The table is non-empty and the index accessor bounds correctly.
static void test_size_and_bounds(void)
{
    size_t n = bacnet_object_map_size();
    TEST_ASSERT_GREATER_THAN_size_t(0, n);
    TEST_ASSERT_NOT_NULL(bacnet_object_map_get(0));
    TEST_ASSERT_NULL(bacnet_object_map_get(n)); // out of range
}

// Every instance is unique and lookup() resolves back to the same row.
static void test_unique_instances_and_lookup(void)
{
    size_t n = bacnet_object_map_size();
    for (size_t i = 0; i < n; i++) {
        const bacnet_object_map_entry_t *e = bacnet_object_map_get(i);
        TEST_ASSERT_NOT_NULL(e);
        // lookup returns *a* row with this instance; assert it's this exact row
        // (would fail if a second row reused the instance and shadowed it).
        TEST_ASSERT_EQUAL_PTR(e, bacnet_object_map_lookup(e->instance));
    }
    // Cross-check uniqueness explicitly (O(n^2), n is tiny).
    for (size_t i = 0; i < n; i++) {
        for (size_t j = i + 1; j < n; j++) {
            TEST_ASSERT_NOT_EQUAL(bacnet_object_map_get(i)->instance,
                                  bacnet_object_map_get(j)->instance);
        }
    }
}

// Unmapped instances resolve to NULL.
static void test_lookup_unknown(void)
{
    TEST_ASSERT_NULL(bacnet_object_map_lookup(99999));
    TEST_ASSERT_NULL(bacnet_object_map_lookup(150)); // equipment range, unused
}

// Each row is well-formed: names present, COV increment non-negative, and the
// instance sits in the documented range for its kind.
static void test_rows_well_formed(void)
{
    for (size_t i = 0; i < bacnet_object_map_size(); i++) {
        const bacnet_object_map_entry_t *e = bacnet_object_map_get(i);
        TEST_ASSERT_NOT_NULL(e->object_name);
        TEST_ASSERT_NOT_NULL(e->description);
        TEST_ASSERT_TRUE(e->cov_increment >= 0.0f);

        // Diagnostics live at 300–399 and carry no space binding; everything
        // else in this milestone is space-aggregated (0–99) with a space id.
        if (e->instance >= 300 && e->instance <= 399) {
            TEST_ASSERT_EQUAL_size_t(0, strlen(e->source_space_id));
        } else {
            TEST_ASSERT_LESS_THAN_UINT32(100, e->instance);
            TEST_ASSERT_GREATER_THAN_size_t(0, strlen(e->source_space_id));
        }
    }
}

// Known anchor objects exist with the expected type + category.
static void test_known_objects(void)
{
    const bacnet_object_map_entry_t *temp = bacnet_object_map_lookup(0);
    TEST_ASSERT_NOT_NULL(temp);
    TEST_ASSERT_EQUAL_INT(OBJ_ANALOG_INPUT, temp->type);
    TEST_ASSERT_EQUAL_INT(DATA_CAT_TEMPERATURE, temp->source_category);
    TEST_ASSERT_TRUE(temp->cov_enabled);

    const bacnet_object_map_entry_t *dc = bacnet_object_map_lookup(10);
    TEST_ASSERT_NOT_NULL(dc);
    TEST_ASSERT_EQUAL_INT(OBJ_BINARY_INPUT, dc->type);

    const bacnet_object_map_entry_t *nvs = bacnet_object_map_lookup(303);
    TEST_ASSERT_NOT_NULL(nvs);
    TEST_ASSERT_EQUAL_INT(OBJ_ANALOG_INPUT, nvs->type);
    TEST_ASSERT_FALSE(nvs->cov_enabled);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_size_and_bounds);
    RUN_TEST(test_unique_instances_and_lookup);
    RUN_TEST(test_lookup_unknown);
    RUN_TEST(test_rows_well_formed);
    RUN_TEST(test_known_objects);
    return UNITY_END();
}
