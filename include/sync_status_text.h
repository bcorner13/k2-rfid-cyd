#pragma once
/**
 * @file sync_status_text.h
 * @brief One-line "live / offline, synced <when>" text for the inventory and settings screens.
 *
 * Cached data is never shown as if it were live (offline-inventory spec, anti-goals): every view
 * of the local copy carries this line. Uses local time (TZ set by configTzTime on the board).
 * ASCII only: LVGL's built-in Montserrat fonts have no middle dot. Pure; host-tested.
 */
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <sync_state.h>

inline void formatSyncStatus(SyncState s, int64_t pullEpoch, uint32_t pullMillis, uint32_t nowMillis,
                             int64_t nowEpoch, char* out, size_t n) {
    (void)nowEpoch;
    switch (s) {
        case SyncState::NoStore:     std::snprintf(out, n, "No SD card - offline mode unavailable"); return;
        case SyncState::NeverSynced: std::snprintf(out, n, "Never synced"); return;
        case SyncState::Syncing:     std::snprintf(out, n, "Syncing..."); return;
        default: break;
    }
    const char* prefix = s == SyncState::Online ? "Live" : "Offline";

    if (pullEpoch > PULL_EPOCH_UNKNOWN) {
        static const char* MONTHS[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
        const time_t t = static_cast<time_t>(pullEpoch);
        struct tm lt;
        localtime_r(&t, &lt);
        if (s == SyncState::Online) {
            std::snprintf(out, n, "%s - synced %02d:%02d", prefix, lt.tm_hour, lt.tm_min);
        } else {
            std::snprintf(out, n, "%s - synced %s %d %02d:%02d", prefix, MONTHS[lt.tm_mon % 12], lt.tm_mday,
                          lt.tm_hour, lt.tm_min);
        }
        return;
    }
    if (pullMillis == 0) {   // pulled on an earlier boot, before the clock was set
        std::snprintf(out, n, "%s - synced before restart", prefix);
        return;
    }
    const uint32_t mins = (nowMillis - pullMillis) / 60000;
    if (mins == 0) std::snprintf(out, n, "%s - synced just now", prefix);
    else std::snprintf(out, n, "%s - synced %u min ago", prefix, static_cast<unsigned>(mins));
}
