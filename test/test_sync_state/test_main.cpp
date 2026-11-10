// Host tests for sync timing: pio test -e native
#include <unity.h>
#include "sync_state.h"

void setUp() {}
void tearDown() {}

void test_online_interval_is_five_minutes() {
    TEST_ASSERT_EQUAL_UINT32(300000, nextSyncDelayMs(SyncState::Online, 0));
}

void test_offline_backoff_doubles_to_cap() {
    TEST_ASSERT_EQUAL_UINT32(30000, nextSyncDelayMs(SyncState::Offline, 1));
    TEST_ASSERT_EQUAL_UINT32(60000, nextSyncDelayMs(SyncState::Offline, 2));
    TEST_ASSERT_EQUAL_UINT32(120000, nextSyncDelayMs(SyncState::Offline, 3));
    TEST_ASSERT_EQUAL_UINT32(300000, nextSyncDelayMs(SyncState::Offline, 4));
    TEST_ASSERT_EQUAL_UINT32(300000, nextSyncDelayMs(SyncState::Offline, 40));
}

void test_never_synced_retries_like_offline() {
    TEST_ASSERT_EQUAL_UINT32(30000, nextSyncDelayMs(SyncState::NeverSynced, 1));
    TEST_ASSERT_EQUAL_UINT32(30000, nextSyncDelayMs(SyncState::NeverSynced, 0));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_online_interval_is_five_minutes);
    RUN_TEST(test_offline_backoff_doubles_to_cap);
    RUN_TEST(test_never_synced_retries_like_offline);
    return UNITY_END();
}
