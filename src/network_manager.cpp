#include <network_manager.h> // Updated include path

#include <LittleFS.h>

#include <filament_db.h> // Updated include path
#include <config_manager.h> // Updated include path
// #include <SPIFFS.h> // This include is inconsistent, removing it

AppNetwork network;

AppNetwork::AppNetwork() {}

void AppNetwork::init() {
    // WiFiManager handles mode automatically
}

bool AppNetwork::connect() {
    // Try to connect with saved creds, non-blocking
    WiFi.mode(WIFI_STA);
    if (WiFi.SSID().length() > 0) {
        WiFi.begin();
        return true;
    }
    return false;
}

void AppNetwork::startConfigPortal() {
    // Add custom parameter for Printer IP
    WiFiManagerParameter custom_printer_ip("printer_ip", "Printer IP", config.data.printer_ip.c_str(), 40);
    wm.addParameter(&custom_printer_ip);

    // Set title
    wm.setTitle("K2 RFID Tool Setup");

    // Start Portal (Blocking!)
    // We should probably show a screen on the LCD before calling this
    if (!wm.startConfigPortal("K2-RFID-SETUP")) {
        Serial.println("failed to connect and hit timeout");
        delay(3000);
        ESP.restart();
    }

    // Save custom param
    config.data.printer_ip = custom_printer_ip.getValue();
    config.save();

    Serial.println("connected...yeey :)");
}

bool AppNetwork::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

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