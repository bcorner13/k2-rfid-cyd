#pragma once
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiManager.h>

// Hard limit per FSD Section 14.2
static constexpr size_t DB_MAX_SIZE_BYTES = 512 * 1024;  // 512 KB
static constexpr int32_t DB_CONNECT_TIMEOUT_MS = 2000;     // printer TCP connect, Update DB

// P1.10: Update result with details for UI display
struct DBUpdateResult {
    bool success;
    bool skipped;           // true if hash matched (already up to date)
    uint16_t profile_count;
    String message;
};

class AppNetwork {
public:
    AppNetwork();
    void init();
    void process();
    bool connect();
    void startConfigPortal();
    void stopConfigPortal();   // no-op when the portal isn't running
    /** Setup WiFi is finished: the portal saved a network and we're on it, or we were offline
     *  when it opened and are now connected. Being connected already at open doesn't count. */
    bool portalDone();
    void reset();
    bool isConnected();

    bool updateFilamentDB();

    // P1.10: Enhanced update with hash check and schema validation
    DBUpdateResult updateFilamentDBWithValidation();

    // Last error message (set by updateFilamentDB on failure)
    const String& getLastError() const { return _lastError; }

private:
    HTTPClient http;
    WiFiManager wm;
    WiFiManagerParameter* custom_printer_ip{nullptr};
    WiFiManagerParameter* custom_spoolman_url{nullptr};
    String _lastError;
    bool portalStartedConnected = false;   // already on WiFi when Setup WiFi opened
    bool portalSavedWifi = false;          // portal saved new network credentials

    // P1.10: SHA-256 hash of data buffer, returned as 64-char hex string
    static String computeSHA256(const uint8_t* data, size_t length);

    // P1.10: Validate expected schema structure
    static bool validateDBSchema(JsonDocument& doc, uint16_t& profileCount);

    // K2 FW 1.1.5.5+: per-model DB lives under /downloads/defData/material/.
    // Builds http://{ip}/downloads/defData/material/{model}_material_database.json
    // (or .../material_database.json when model is empty).
    static String buildDBURL(const String& printer_ip, const String& printer_model);
};

extern AppNetwork network;