#pragma once
/**
 * @file atomic_write.h
 * @brief Atomic JSON write and crash-recovery utilities for LittleFS.
 *
 * Provides a 6-step atomic write (tmp → flush → close → remove → rename)
 * and a recovery function to clean up partial writes after power loss.
 * Used by both ConfigManager and InventoryManager.
 */

#include <ArduinoJson.h>
#include <LittleFS.h>

/**
 * Atomically write a JsonDocument to a LittleFS path.
 *
 * Steps: write to {path}.tmp, flush, close, remove original, rename tmp.
 * Returns true on success, false (with Serial logging) on any failure.
 */
inline bool atomicWriteJson(const char* path, JsonDocument& doc) {
    String tmpPath = String(path) + ".tmp";

    // Step 1: Open tmp file for write
    File file = LittleFS.open(tmpPath.c_str(), "w");
    if (!file) {
        Serial.printf("ERROR: atomicWrite: failed to open %s\n", tmpPath.c_str());
        return false;
    }

    // Step 2: Serialize JSON to tmp file
    size_t written = serializeJson(doc, file);
    if (written == 0) {
        Serial.printf("ERROR: atomicWrite: serializeJson wrote 0 bytes to %s\n", tmpPath.c_str());
        file.close();
        LittleFS.remove(tmpPath.c_str());
        return false;
    }

    // Step 3: Flush
    file.flush();

    // Step 4: Close
    file.close();

    // Step 5: Remove original
    if (LittleFS.exists(path)) {
        if (!LittleFS.remove(path)) {
            Serial.printf("ERROR: atomicWrite: failed to remove original %s\n", path);
            return false;
        }
    }

    // Step 6: Rename tmp to original
    if (!LittleFS.rename(tmpPath.c_str(), path)) {
        Serial.printf("ERROR: atomicWrite: failed to rename %s -> %s\n", tmpPath.c_str(), path);
        return false;
    }

    return true;
}

/**
 * Recover from a partial atomic write after power loss.
 *
 * - .tmp exists + original exists -> delete .tmp (write was incomplete)
 * - .tmp exists + original missing -> rename .tmp to original (write completed step 5 but not 6)
 * - No .tmp -> no-op
 */
inline void recoverTmpFile(const char* path) {
    String tmpPath = String(path) + ".tmp";

    if (!LittleFS.exists(tmpPath.c_str())) {
        return; // No recovery needed
    }

    if (LittleFS.exists(path)) {
        // Original exists, tmp is leftover from incomplete write
        Serial.printf("RECOVERY: removing stale tmp file %s\n", tmpPath.c_str());
        LittleFS.remove(tmpPath.c_str());
    } else {
        // Original missing, tmp contains the completed write
        Serial.printf("RECOVERY: promoting tmp file %s -> %s\n", tmpPath.c_str(), path);
        LittleFS.rename(tmpPath.c_str(), path);
    }
}
