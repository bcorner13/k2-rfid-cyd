#pragma once
#include <HTTPClient.h>
#include <WiFiManager.h>

// Hard limit per FSD Section 14.2
static constexpr size_t DB_MAX_SIZE_BYTES = 512 * 1024;  // 512 KB

class AppNetwork {
public:
    AppNetwork();
    void init();
    bool connect();
    void startConfigPortal();
    bool isConnected();

    bool updateFilamentDB();

    // Last error message (set by updateFilamentDB on failure)
    const String& getLastError() const { return _lastError; }

private:
    HTTPClient http;
    WiFiManager wm;
    String _lastError;
};

extern AppNetwork network;