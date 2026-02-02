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
    if (!isConnected()) {
        // Try to connect if not already
        if (!connect()) return false;
        // Wait a bit
        int timeout = 20;
        while (WiFi.status() != WL_CONNECTED && timeout > 0) {
            delay(500);
        }
        if (WiFi.status() != WL_CONNECTED) return false;
    }
    // This wont work as on the printer they use an actual data.db file.
    String url = "http://" + config.data.printer_ip + "/material_database.json";
    // Or use the specific API path if different

    Serial.println("Updating from: " + url);

    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        File file = LittleFS.open("/material_database.json", "w");
        if (!file) {
            http.end();
            return false;
        }

        http.writeToStream(&file);
        file.close();
        http.end();

        filamentDB.init();
        return true;
    }

    Serial.printf("HTTP Failed: %d\n", httpCode);
    http.end();
    return false;
}