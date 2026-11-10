#pragma once
/**
 * @file spoolman_sync.h
 * @brief Background sync between Spoolman and the local copy on SD.
 *
 * Its own FreeRTOS task owns all Spoolman traffic for the mirror: pull (and, from plan Task 8,
 * push of pending changes). The UI never waits on it; it reads state() and calls kick() after a
 * local change. Live lookups by tag stay in SpoolmanClient.
 * Timing: every 5 min while online, at once after kick(); offline back-off 30/60/120/300 s.
 */
#include <Arduino.h>
#include <sync_state.h>

class SpoolmanSync {
public:
    void init();                          // reads the base URL from config, starts task "spoolsync"
    void setBaseUrl(const char* url);     // portal saved a new URL
    void kick();                          // a local change happened, or "Sync now": sync at once

    SyncState state() const;
    int64_t lastPullEpoch() const;        // 0 never, PULL_EPOCH_UNKNOWN = pulled before the clock was set
    uint32_t lastPullMillis() const;      // millis() of the last pull this boot, 0 = none
    uint32_t changeCounter() const;       // bumps after every successful pull: UI refreshes on change

private:
    static void taskMain(void* arg);
    void runOnce();

    TaskHandle_t task_ = nullptr;
};

extern SpoolmanSync spoolmanSync;
