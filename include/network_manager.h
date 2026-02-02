#pragma once
#include <HTTPClient.h>
#include <WiFiManager.h>

class AppNetwork {
public:
    AppNetwork();
    void init();
    bool connect(); // Auto connect using saved creds
    void startConfigPortal(); // Force portal
    bool isConnected();

    // Update DB from URL
    bool updateFilamentDB();

private:
    HTTPClient http;
    WiFiManager wm;
};

extern AppNetwork network;