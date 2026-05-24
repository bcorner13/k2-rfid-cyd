#pragma once
#include <HTTPClient.h>
#include <WiFiManager.h>

// Hard limit per FSD Section 14.2
static constexpr size_t DB_MAX_SIZE_BYTES = 512 * 1024;  // 512 KB

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
    String _lastError;

    // P1.10: SHA-256 hash of data buffer, returned as 64-char hex string
    static String computeSHA256(const uint8_t* data, size_t length);

    // P1.10: Validate expected schema structure
    static bool validateDBSchema(const uint8_t* data, size_t length, uint16_t& profileCount);

    // K2 FW 1.1.5.5+: per-model DB lives under /downloads/defData/material/.
    // Builds http://{ip}/downloads/defData/material/{model}_material_database.json
    // (or .../material_database.json when model is empty).
    static String buildDBURL(const String& printer_ip, const String& printer_model);
};

extern AppNetwork network;