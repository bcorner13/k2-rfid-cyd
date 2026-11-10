// Host tests for the sync status line on the inventory/settings screens: pio test -e native
#include <unity.h>
#include <cstdlib>
#include <ctime>
#include <string>
#include "sync_status_text.h"

void setUp() {
    setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);
    tzset();
}
void tearDown() {}

// 2026-11-09 14:05:00 EST = 19:05:00 UTC
static const int64_t PULL = 1794251100;
static const int64_t NOW = PULL + 600;

static std::string fmt(SyncState s, int64_t pullEpoch, uint32_t pullMs, uint32_t nowMs, int64_t nowEpoch) {
    char buf[64];
    formatSyncStatus(s, pullEpoch, pullMs, nowMs, nowEpoch, buf, sizeof(buf));
    return buf;
}

void test_online_shows_time() {
    TEST_ASSERT_EQUAL_STRING("Live - synced 14:05", fmt(SyncState::Online, PULL, 1000, 601000, NOW).c_str());
}

void test_offline_shows_date_and_time() {
    TEST_ASSERT_EQUAL_STRING("Offline - synced Nov 9 14:05", fmt(SyncState::Offline, PULL, 0, 5000, NOW).c_str());
}

void test_offline_no_clock_minutes_ago() {
    TEST_ASSERT_EQUAL_STRING("Offline - synced 12 min ago",
                             fmt(SyncState::Offline, PULL_EPOCH_UNKNOWN, 60000, 60000 + 12 * 60000, 0).c_str());
}

void test_no_clock_just_now() {
    TEST_ASSERT_EQUAL_STRING("Live - synced just now",
                             fmt(SyncState::Online, PULL_EPOCH_UNKNOWN, 60000, 90000, 0).c_str());
}

void test_offline_no_clock_previous_boot() {
    TEST_ASSERT_EQUAL_STRING("Offline - synced before restart",
                             fmt(SyncState::Offline, PULL_EPOCH_UNKNOWN, 0, 5000, 0).c_str());
}

void test_never_synced() {
    TEST_ASSERT_EQUAL_STRING("Never synced", fmt(SyncState::NeverSynced, 0, 0, 5000, NOW).c_str());
}

void test_no_store() {
    TEST_ASSERT_EQUAL_STRING("No SD card - offline mode unavailable", fmt(SyncState::NoStore, PULL, 0, 5000, NOW).c_str());
}

void test_syncing() {
    TEST_ASSERT_EQUAL_STRING("Syncing...", fmt(SyncState::Syncing, PULL, 0, 5000, NOW).c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_online_shows_time);
    RUN_TEST(test_offline_shows_date_and_time);
    RUN_TEST(test_offline_no_clock_minutes_ago);
    RUN_TEST(test_no_clock_just_now);
    RUN_TEST(test_offline_no_clock_previous_boot);
    RUN_TEST(test_never_synced);
    RUN_TEST(test_no_store);
    RUN_TEST(test_syncing);
    return UNITY_END();
}
