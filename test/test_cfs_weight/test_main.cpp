// Host tests for CFS spool-size steps: pio test -e native
#include <unity.h>
#include "cfs_weight.h"

void setUp() {}
void tearDown() {}

// The five values DnG v57 writes, both directions.
void test_steps_match_dng() {
    TEST_ASSERT_EQUAL_UINT32(82,  cfs::weightToLength(250));
    TEST_ASSERT_EQUAL_UINT32(165, cfs::weightToLength(500));
    TEST_ASSERT_EQUAL_UINT32(165, cfs::weightToLength(600));   // 600 not offered -> 500
    TEST_ASSERT_EQUAL_UINT32(247, cfs::weightToLength(750));
    TEST_ASSERT_EQUAL_UINT32(330, cfs::weightToLength(1000));
    TEST_ASSERT_EQUAL_UINT32(750, cfs::lengthToWeight(247));   // linear formula gave 748
    TEST_ASSERT_EQUAL_UINT32(250, cfs::lengthToWeight(82));    // linear formula gave 248
    TEST_ASSERT_EQUAL_UINT32(500, cfs::lengthToWeight(165));   // genuine Creality spool
    TEST_ASSERT_EQUAL_UINT32(600, cfs::lengthToWeight(198));   // genuine 600 g tag still decodes
}

void test_snap_rounds_down() {
    TEST_ASSERT_EQUAL_UINT32(750,  cfs::snapWeight(800));      // SimplyPrint's example
    TEST_ASSERT_EQUAL_UINT32(500,  cfs::snapWeight(599));
    TEST_ASSERT_EQUAL_UINT32(1000, cfs::snapWeight(1000));
    TEST_ASSERT_EQUAL_UINT32(1000, cfs::snapWeight(3000));
}

void test_edges() {
    TEST_ASSERT_EQUAL_UINT32(250,  cfs::snapWeight(100));      // below the smallest step
    TEST_ASSERT_EQUAL_UINT32(1000, cfs::snapWeight(0));        // unknown -> 1 kg default
    TEST_ASSERT_EQUAL_UINT32(800,  cfs::lengthToWeight(264));  // off-step legacy card, linear
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_steps_match_dng);
    RUN_TEST(test_snap_rounds_down);
    RUN_TEST(test_edges);
    return UNITY_END();
}
