#include <network_manager.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>
#include <filament_db.h>
#include <config_manager.h>
#include "mbedtls/sha256.h"

AppNetwork network;

AppNetwork::AppNetwork() {}

void AppNetwork::init() {
    wm.setDebugOutput(true);
    wm.setConfigPortalBlocking(false);  // Non-blocking portal!

    // Set callback to save custom params
    wm.setSaveParamsCallback([this]() {
        if (this->custom_printer_ip) {
            config.data.printer_ip = this->custom_printer_ip->getValue();
            config.save();
            Serial.println("Printer IP saved from WiFi Portal");
        }
    });
}

void AppNetwork::process() {
    wm.process();
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
bool AppNetwork::validateDBSchema(const uint8_t* data, size_t length, uint16_t& profileCount) {
    profileCount = 0;

    // Parse into a temporary doc to check structure
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, (const char*)data, length);
    if (err) {
        Serial.printf("Schema validation: JSON parse failed: %s\n", err.c_str());
        return false;
    }

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
DBUpdateResult AppNetwork::updateFilamentDBWithValidation() {
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
    String url = "http://" + config.data.printer_ip + "/downloads/defData/material_database.json";
    Serial.println("Updating from: " + url);

    http.begin(url);
    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        result.message = "Cannot reach printer (HTTP " + String(httpCode) + ")";
        _lastError = result.message;
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

    Serial.printf("Downloaded %u bytes\n", totalRead);

    // FSD 5.6 step 5: Compute SHA-256
    String newHash = computeSHA256(dbBuf, totalRead);
    Serial.printf("SHA-256: %s\n", newHash.c_str());

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

    // FSD 5.6 step 7: Validate schema
    uint16_t profileCount = 0;
    bool schemaOk = validateDBSchema(dbBuf, totalRead, profileCount);

    // Write to temp file for atomic replace
    File file = LittleFS.open("/material_database.json.tmp", "w");
    if (!file) {
        free(dbBuf);
        result.message = "Storage error: cannot create temp file";
        _lastError = result.message;
        return result;
    }

    file.write(dbBuf, totalRead);
    file.flush();
    file.close();
    free(dbBuf);

    // Atomic replace
    if (LittleFS.exists("/material_database.json")) {
        LittleFS.remove("/material_database.json");
    }
    if (!LittleFS.rename("/material_database.json.tmp", "/material_database.json")) {
        result.message = "Storage error: rename failed";
        _lastError = result.message;
        return result;
    }

    // FSD 5.6 step 10: Reload database cache
    filamentDB.init();

    // FSD 5.6 step 11: Update config metadata
    config.data.db_hash = newHash;
    config.data.db_updated_at = millis() / 1000;
    config.data.db_profile_count = profileCount;
    config.data.db_schema_version = schemaOk ? 1 : 0;
    config.save();

    result.success = true;
    result.skipped = false;
    result.profile_count = profileCount;
    if (schemaOk) {
        result.message = "Updated: " + String(profileCount) + " profiles";
    } else {
        result.message = "Updated (unknown schema): " + String(totalRead) + " bytes";
    }

    Serial.println(result.message);
    return result;
}

// ---------------------------------------------------------------------------
// Legacy updateFilamentDB (delegates to validated version)
// ---------------------------------------------------------------------------
bool AppNetwork::updateFilamentDB() {
    _lastError = "";

    if (!isConnected()) {
        if (!connect()) {
            _lastError = "WiFi not connected";
            return false;
        }
        int timeout = 40;  // 20 seconds
        while (WiFi.status() != WL_CONNECTED && timeout > 0) {
            delay(500);
            timeout--;
        }
        if (WiFi.status() != WL_CONNECTED) {
            _lastError = "WiFi connection timed out";
            return false;
        }
    }

    // FSD Section 5.6: printer API endpoint
    String url = "http://" + config.data.printer_ip + "/downloads/defData/material_database.json";
    Serial.println("Updating from: " + url);

    http.begin(url);
    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        _lastError = "HTTP error: " + String(httpCode);
        Serial.printf("HTTP failed: %d\n", httpCode);
        http.end();
        return false;
    }

    // P0.4: Content-Length guard (FSD Section 14.2)
    int contentLength = http.getSize();
    if (contentLength > (int)DB_MAX_SIZE_BYTES) {
        _lastError = "Database too large (" + String(contentLength / 1024) + " KB, max 512 KB)";
        Serial.println(_lastError);
        http.end();
        return false;
    }

    // Content-Length -1 means unknown (chunked transfer) — allow but cap during write
    if (contentLength > 0) {
        Serial.printf("Database size: %d bytes\n", contentLength);
    } else {
        Serial.println("Content-Length unknown, will cap at 512 KB");
    }

    // Write to temp file for atomic replace
    File file = LittleFS.open("/material_database.json.tmp", "w");
    if (!file) {
        _lastError = "Storage error: cannot create temp file";
        http.end();
        return false;
    }

    // Stream response to file with size cap
    WiFiClient* stream = http.getStreamPtr();
    size_t totalWritten = 0;
    uint8_t buf[512];
    bool oversized = false;

    while (http.connected() && (contentLength > 0 || contentLength == -1)) {
        size_t available = stream->available();
        if (available == 0) {
            delay(1);
            continue;
        }
        size_t toRead = (available > sizeof(buf)) ? sizeof(buf) : available;
        size_t bytesRead = stream->readBytes(buf, toRead);

        if (totalWritten + bytesRead > DB_MAX_SIZE_BYTES) {
            oversized = true;
            break;
        }

        file.write(buf, bytesRead);
        totalWritten += bytesRead;

        if (contentLength > 0) {
            contentLength -= bytesRead;
        }
    }

    file.flush();
    file.close();
    http.end();

    if (oversized) {
        LittleFS.remove("/material_database.json.tmp");
        _lastError = "Database too large (exceeded 512 KB during download)";
        Serial.println(_lastError);
        return false;
    }

    if (totalWritten == 0) {
        LittleFS.remove("/material_database.json.tmp");
        _lastError = "Empty response from printer";
        return false;
    }

    // Atomic replace: remove old, rename temp
    if (LittleFS.exists("/material_database.json")) {
        LittleFS.remove("/material_database.json");
    }
    if (!LittleFS.rename("/material_database.json.tmp", "/material_database.json")) {
        _lastError = "Storage error: rename failed";
        return false;
    }

    Serial.printf("Database updated: %u bytes written\n", totalWritten);

    // Reload database cache
    filamentDB.init();
    return true;
}