#include <sd_storage.h>
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "board_pins.h"

SdStorage sdStorage;

namespace {
SPIClass sdSpi(FSPI);
constexpr const char* DB_PATH = "/sd/k2rfid.db";
}  // namespace

bool SdStorage::mount() {
    if (mountTried_) return mounted_;
    mountTried_ = true;
    sdSpi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    mounted_ = SD.begin(PIN_SD_CS, sdSpi, 20000000, "/sd");
    if (mounted_)
        Serial.printf("SD: %llu MB card, %llu KB used\n", SD.cardSize() / (1024 * 1024), SD.usedBytes() / 1024);
    else
        Serial.println("SD: no card");
    return mounted_;
}

bool SdStorage::begin() {
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) {
        markFailed("no mutex");
        return false;
    }
    if (!mount()) {
        markFailed("no card");
        Serial.println("SD: no card - offline mode unavailable");
        return false;
    }

    const uint32_t t0 = millis();
    if (!store_.open(DB_PATH)) {
        markFailed("database open failed");
        Serial.printf("SD: cannot open %s - offline mode unavailable\n", DB_PATH);
        return false;
    }
    available_ = true;
    snprintf(failure_, sizeof(failure_), "%s", "");
    Serial.printf("SD: %s open (db %s, %d pending, %d need attention) in %lu ms\n", DB_PATH,
                  store_.dbId().c_str(), store_.pendingCount(), store_.attentionCount(),
                  (unsigned long)(millis() - t0));
    return true;
}

void SdStorage::markFailed(const char* why) {
    if (available_) Serial.printf("SD: offline mode off: %s\n", why);
    available_ = false;
    snprintf(failure_, sizeof(failure_), "%s", why);
}

LocalStore* SdStorage::lock(uint32_t waitMs) {
    if (!available_ || !mutex_) return nullptr;
    if (xSemaphoreTake(mutex_, waitMs == portMAX_DELAY ? portMAX_DELAY : pdMS_TO_TICKS(waitMs)) != pdTRUE) return nullptr;
    if (!available_) {       // failed while we waited
        xSemaphoreGive(mutex_);
        return nullptr;
    }
    return &store_;
}

void SdStorage::unlock() {
    if (mutex_) xSemaphoreGive(mutex_);
}
