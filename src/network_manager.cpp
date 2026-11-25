#include <spoolman_sync.h>
#include <network_manager.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>
#include <filament_db.h>
#include <config_manager.h>
#include <spoolman_parse.h>
#include "mbedtls/sha256.h"
#include <psram_allocator.h>
#include <material_db_merge.h>
#include <bundled_colors.h>
#include <esp_task_wdt.h>

AppNetwork network;

AppNetwork::AppNetwork() {}

void AppNetwork::init() {
    wm.setDebugOutput(true);
    wm.setConfigPortalBlocking(false);  // Non-blocking portal!

    // Set callback to save custom params
    wm.setSaveParamsCallback([this]() {
        if (this->custom_printer_ip) {
            config.data.printer_ip = this->custom_printer_ip->getValue();
        }
        if (this->custom_spoolman_url) {
            config.data.spoolman_url = this->custom_spoolman_url->getValue();
            config.data.spoolman_url.trim();
        }
        config.save();
        spoolmanSync.setBaseUrl(config.data.spoolman_url.c_str());
        spoolmanSync.kick();
        Serial.println("Printer IP and Spoolman URL saved from WiFi Portal");
    });
}

void AppNetwork::process() {
    wm.process();
    // Clock for "synced <time>" labels; there is no RTC driver yet (offline-inventory spec).
    static bool clockStarted = false;
    if (!clockStarted && WiFi.status() == WL_CONNECTED) {
        configTzTime("EST5EDT,M3.2.0,M11.1.0", "pool.ntp.org", "time.nist.gov");
        clockStarted = true;
    }
}

bool AppNetwork::connect() {
    // WiFi.SSID() only returns a value when already connected, NOT from NVS on cold
    // boot — so don't gate on it. WiFi.begin() with no args reconnects to the last
    // saved network from NVS regardless of current connection state.
    WiFi.persistent(true);        // Ensure credentials are written to NVS on connect
    WiFi.setAutoReconnect(true);  // Auto-reconnect if connection drops mid-session
    WiFi.mode(WIFI_STA);
    WiFi.begin();                 // Reconnect using stored NVS credentials
    return true;
}

void AppNetwork::startConfigPortal() {
    // Ensure parameter object exists and is added only once
    if (!custom_printer_ip) {
        custom_printer_ip = new WiFiManagerParameter("printer_ip", "Printer IP", config.data.printer_ip.c_str(), 40);
        wm.addParameter(custom_printer_ip);
    }
    if (!custom_spoolman_url) {
        custom_spoolman_url = new WiFiManagerParameter("spoolman_url", "Spoolman URL (blank = off)",
                                                       config.data.spoolman_url.c_str(), spoolman::BASE_URL_BUF - 1);
        wm.addParameter(custom_spoolman_url);
    }

    wm.setTitle("K2 RFID Tool Setup");
    wm.startConfigPortal("K2-RFID-SETUP");
}

void AppNetwork::reset() {
    Serial.println("WiFi: Clearing settings and restarting...");
    wm.resetSettings();
    WiFi.disconnect(true, true); // Clear SSID/Password from NVS
    delay(1000);
    ESP.restart();
}

bool AppNetwork::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

// ---------------------------------------------------------------------------
// K2 FW 1.1.5.5 moved material DB to /downloads/defData/material/ and split
// it per-printer-model. Build the right URL for the configured model.
// ---------------------------------------------------------------------------
String AppNetwork::buildDBURL(const String& printer_ip, const String& printer_model) {
    String url = "http://" + printer_ip + "/downloads/defData/material/";
    if (printer_model.length() > 0) {
        url += printer_model + "_";
    }
    url += "material_database.json";
    return url;
}

// ---------------------------------------------------------------------------
// P1.10: SHA-256 hash using ESP32 hardware-accelerated mbedtls
// ---------------------------------------------------------------------------
String AppNetwork::computeSHA256(const uint8_t* data, size_t length) {
    uint8_t hash[32];
    mbedtls_sha256(data, length, hash, 0);  // 0 = SHA-256 (not SHA-224)

    char hex[65];
    for (int i = 0; i < 32; i++) {
        snprintf(hex + i * 2, 3, "%02x", hash[i]);
    }
    hex[64] = '\0';
    return String(hex);
}

// ---------------------------------------------------------------------------
// P1.10: Validate expected DB schema — checks for result.list[] with base.id
// ---------------------------------------------------------------------------
bool AppNetwork::validateDBSchema(JsonDocument& doc, uint16_t& profileCount) {
    profileCount = 0;

    // Check for expected structure: result.list[] array
    JsonArray list = doc["result"]["list"].as<JsonArray>();
    if (list.isNull()) {
        Serial.println("Schema validation: missing result.list[] array");
        return false;
    }

    // Spot-check first entry for expected fields
    if (list.size() > 0) {
        JsonObject first = list[0];
        if (first["base"].isNull()) {
            Serial.println("Schema validation: first entry missing 'base' key");
            return false;
        }
        JsonObject base = first["base"];
        if (base["id"].isNull() || base["brand"].isNull() || base["name"].isNull()) {
            Serial.println("Schema validation: base missing id/brand/name");
            return false;
        }
    }

    profileCount = list.size();
    Serial.printf("Schema validation OK: %u profiles\n", profileCount);
    return true;
}

// ---------------------------------------------------------------------------
// P1.10: Enhanced DB update with SHA-256 hash check and schema validation
// ---------------------------------------------------------------------------
// Update DB runs on the loop task, which has a 20 s task watchdog (main.cpp). Download + parse +
// merge + a 400 KB LittleFS write + reload can take longer (the board rebooted mid-update on
// 2026-11-25), so the task leaves the watchdog for the update, as FilamentDB's load already does.
// filamentDB.init() re-adds it near the end; adding twice is harmless.
namespace {
struct WdtPause {
    TaskHandle_t t = xTaskGetCurrentTaskHandle();
    WdtPause()  { esp_task_wdt_delete(t); }
    ~WdtPause() { esp_task_wdt_add(t); esp_task_wdt_reset(); }
};
}  // namespace

DBUpdateResult AppNetwork::updateFilamentDBWithValidation() {
    WdtPause wdtPause;
    const uint32_t tStart = millis();
    DBUpdateResult result = {false, false, 0, ""};
    _lastError = "";

    if (!isConnected()) {
        if (!connect()) {
            result.message = "WiFi not connected";
            _lastError = result.message;
            return result;
        }
        int timeout = 40;
        while (WiFi.status() != WL_CONNECTED && timeout > 0) {
            delay(500);
            timeout--;
        }
        if (WiFi.status() != WL_CONNECTED) {
            result.message = "WiFi connection timed out";
            _lastError = result.message;
            return result;
        }
    }

    // FSD Section 5.6 step 4: Download
    String url = buildDBURL(config.data.printer_ip, config.data.printer_model);
    Serial.println("Updating from: " + url);

    http.begin(url);
    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        result.message = "Cannot reach printer (HTTP " + String(httpCode) + ")";
        _lastError = result.message;
        Serial.println("DB update: " + result.message);
        http.end();
        return result;
    }

    // P0.4: Content-Length guard
    int contentLength = http.getSize();
    if (contentLength > (int)DB_MAX_SIZE_BYTES) {
        result.message = "Database too large (" + String(contentLength / 1024) + " KB, max 512 KB)";
        _lastError = result.message;
        http.end();
        return result;
    }

    // Stream into PSRAM buffer for hash + validation before writing
    size_t bufSize = (contentLength > 0) ? contentLength : DB_MAX_SIZE_BYTES;
    uint8_t* dbBuf = (uint8_t*)heap_caps_malloc(bufSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!dbBuf) {
        // Fallback to regular heap
        dbBuf = (uint8_t*)malloc(bufSize);
    }
    if (!dbBuf) {
        result.message = "Not enough memory for download buffer";
        _lastError = result.message;
        http.end();
        return result;
    }

    WiFiClient* stream = http.getStreamPtr();
    size_t totalRead = 0;

    while (http.connected() && (contentLength > 0 || contentLength == -1)) {
        size_t available = stream->available();
        if (available == 0) {
            delay(1);
            continue;
        }
        size_t toRead = (available > 512) ? 512 : available;
        if (totalRead + toRead > DB_MAX_SIZE_BYTES) {
            free(dbBuf);
            result.message = "Database too large (exceeded 512 KB during download)";
            _lastError = result.message;
            http.end();
            return result;
        }
        size_t bytesRead = stream->readBytes(dbBuf + totalRead, toRead);
        totalRead += bytesRead;
        if (contentLength > 0) contentLength -= bytesRead;
    }
    http.end();

    if (totalRead == 0) {
        free(dbBuf);
        result.message = "Empty response from printer";
        _lastError = result.message;
        return result;
    }

    Serial.printf("Downloaded %u bytes (%lu ms)\n", totalRead, (unsigned long)(millis() - tStart));

    // FSD 5.6 step 5: Compute SHA-256
    String newHash = computeSHA256(dbBuf, totalRead);
    Serial.printf("SHA-256: %s\n", newHash.c_str());

    // The merge also takes the firmware's bundled colours, so a new firmware with new colours
    // must re-merge even when the printer's file is unchanged: key the skip on both.
    newHash += "+" + computeSHA256((const uint8_t*)BUNDLED_COLORS_JSON, sizeof(BUNDLED_COLORS_JSON) - 1)
                         .substring(0, 16);

    // FSD 5.6 step 6: Compare hash
    if (newHash == config.data.db_hash && !config.data.db_hash.isEmpty()) {
        free(dbBuf);
        result.success = true;
        result.skipped = true;
        result.profile_count = config.data.db_profile_count;
        result.message = "Database already up to date (" + String(result.profile_count) + " profiles)";
        Serial.println(result.message);
        return result;
    }

    // FSD 5.6 step 7: Validate, then merge into the current DB instead of replacing it.
    // The printer's factory file has fewer profiles and one colour per product (no names);
    // replacing the file outright lost CR-Silk / Ender-PLA and every product colour.
    JsonDocument incoming(psramJsonAllocator());
    DeserializationError perr = deserializeJson(incoming, (const char*)dbBuf, totalRead);
    free(dbBuf);   // the document holds its own copies of the strings
    uint16_t downloadedCount = 0;
    if (perr || !validateDBSchema(incoming, downloadedCount)) {
        result.message = perr ? String("Update refused: printer DB is not valid JSON (") + perr.c_str() + ")"
                              : String("Update refused: printer DB has an unknown format");
        result.message += "; current DB kept";
        _lastError = result.message;
        Serial.println(result.message);
        return result;
    }

    JsonDocument current(psramJsonAllocator());
    {
        File cur = LittleFS.open("/material_database.json", "r");
        if (cur) {
            DeserializationError cerr = deserializeJson(current, cur);
            cur.close();
            if (cerr) {
                Serial.printf("DB update: current DB unreadable (%s), taking the download as is\n", cerr.c_str());
                current.clear();
            }
        }
    }
    // Product colours bundled in the firmware (data/material_database.json): colours the printer's
    // factory file lacks, like Hyper PC Transparent, reach the board without a uploadfs.
    JsonDocument bundled(psramJsonAllocator());
    DeserializationError berr = deserializeJson(bundled, BUNDLED_COLORS_JSON);
    if (berr) Serial.printf("DB merge: bundled colours unreadable (%s), skipped\n", berr.c_str());
    Serial.printf("DB update: current DB parsed (%lu ms)\n", (unsigned long)(millis() - tStart));
    DbMergeStats st;
    mergeMaterialDb(current, incoming, st, berr ? nullptr : &bundled);   // incoming has result.list
    current.clear();
    bundled.clear();
    const uint16_t profileCount = incoming["result"]["list"].size();
    Serial.printf("DB merge: %u updated (%u kept their colours), %u new, %u kept from current, "
                  "%u bundled colours added; %u total. Free PSRAM %u\n", st.updated, st.coloursKept,
                  st.added, st.keptLocalOnly, st.bundledColoursAdded, profileCount,
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    // Write to temp file for atomic replace
    File file = LittleFS.open("/material_database.json.tmp", "w");
    if (!file) {
        result.message = "Storage error: cannot create temp file";
        _lastError = result.message;
        return result;
    }
    Serial.printf("DB update: merged, writing (%lu ms)\n", (unsigned long)(millis() - tStart));
    const size_t written = serializeJson(incoming, file);
    file.flush();
    file.close();
    incoming.clear();
    if (written == 0) {
        LittleFS.remove("/material_database.json.tmp");
        result.message = "Storage error: could not write merged DB";
        _lastError = result.message;
        return result;
    }

    // Atomic replace
    if (LittleFS.exists("/material_database.json")) {
        LittleFS.remove("/material_database.json");
    }
    if (!LittleFS.rename("/material_database.json.tmp", "/material_database.json")) {
        result.message = "Storage error: rename failed";
        _lastError = result.message;
        return result;
    }

    Serial.printf("DB update: written %u bytes, reloading (%lu ms)\n", (unsigned)written,
                  (unsigned long)(millis() - tStart));
    // FSD 5.6 step 10: Reload database cache
    filamentDB.init();
    Serial.printf("DB update: done (%lu ms)\n", (unsigned long)(millis() - tStart));

    // FSD 5.6 step 11: Update config metadata
    config.data.db_hash = newHash;
    config.data.db_updated_at = millis() / 1000;
    config.data.db_profile_count = profileCount;
    config.data.db_schema_version = 1;
    config.save();

    result.success = true;
    result.skipped = false;
    result.profile_count = profileCount;
    result.message = "Updated: " + String(profileCount) + " profiles (" + String(st.added) + " new, " +
                     String(st.keptLocalOnly) + " kept from the device)";

    Serial.println(result.message);
    return result;
}

// ---------------------------------------------------------------------------
// Legacy updateFilamentDB (delegates to validated version)
// ---------------------------------------------------------------------------
bool AppNetwork::updateFilamentDB() {
    return updateFilamentDBWithValidation().success;   // sets _lastError on failure
}
