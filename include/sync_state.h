#pragma once
/**
 * @file sync_state.h
 * @brief Where the local copy of Spoolman stands. Pure, so host tests can use it.
 */
#include <cstdint>

enum class SyncState : uint8_t {
    NoStore,       // no SD card, or the database failed: offline mode unavailable
    NeverSynced,   // store open, never pulled from Spoolman
    Online,        // last sync attempt reached Spoolman
    Offline,       // last sync attempt failed; showing the last pull
    Syncing,       // a sync is running now
};

/// Pull time stored when the clock wasn't set yet: "pulled, time unknown". 0 = never pulled.
constexpr int64_t PULL_EPOCH_UNKNOWN = 1;

/// Wait before the next sync attempt. Online: every 5 min. Otherwise back off after each failure,
/// 30 s -> 60 s -> 120 s -> 300 s cap (`failures` = consecutive failed attempts; 0 counts as 1).
inline uint32_t nextSyncDelayMs(SyncState lastState, uint32_t failures) {
    constexpr uint32_t ONLINE_MS = 300000;
    constexpr uint32_t BACKOFF_MS[] = { 30000, 60000, 120000, 300000 };
    constexpr uint32_t STEPS = sizeof(BACKOFF_MS) / sizeof(BACKOFF_MS[0]);
    if (lastState == SyncState::Online) return ONLINE_MS;
    const uint32_t i = failures == 0 ? 0 : failures - 1;
    return BACKOFF_MS[i < STEPS ? i : STEPS - 1];
}
