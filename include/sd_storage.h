#pragma once
/**
 * @file sd_storage.h
 * @brief SD card mount and the one LocalStore (/sd/k2rfid.db), behind a FreeRTOS mutex.
 *
 * Tag read/write never depends on this: with no card or a failed card, available() is false,
 * every lock() returns nullptr, and the UI shows "No SD card - offline mode unavailable".
 * UI calls wait at most ~50 ms for the lock (the sync task can hold it ~1 s during a pull).
 */
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <local_store.h>

class SdStorage {
public:
    bool begin();                       // mounts SD (SPI 11/12/13, CS 10) and opens /sd/k2rfid.db
    bool available() const { return available_; }
    void markFailed(const char* why);   // a store call failed on the board: offline mode off until reboot
    const char* failure() const { return failure_; }

    LocalStore* lock(uint32_t waitMs);  // nullptr if busy or unavailable
    void unlock();

private:
    LocalStore store_;
    SemaphoreHandle_t mutex_ = nullptr;
    volatile bool available_ = false;
    char failure_[48] = "not started";
};

extern SdStorage sdStorage;

/// RAII lock: `StoreGuard g; if (g) g->listSpools(...)`.
class StoreGuard {
public:
    explicit StoreGuard(uint32_t waitMs = 50) : store_(sdStorage.lock(waitMs)) {}
    ~StoreGuard() { if (store_) sdStorage.unlock(); }
    StoreGuard(const StoreGuard&) = delete;
    StoreGuard& operator=(const StoreGuard&) = delete;
    LocalStore* operator->() const { return store_; }
    explicit operator bool() const { return store_ != nullptr; }

private:
    LocalStore* store_;
};
