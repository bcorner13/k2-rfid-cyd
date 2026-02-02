#include <config_manager.h> // Updated include path
#include <LittleFS.h> // Changed from SPIFFS.h

ConfigManager config;

ConfigManager::ConfigManager() {}

void ConfigManager::init() {
    if(!LittleFS.begin(true)) { // Changed from SPIFFS.begin
        Serial.println("WARNING: LittleFS failed to mount for ConfigManager!");
        return;
    }
    load();
}

void ConfigManager::load() {
    if (!LittleFS.exists("/config.json")) { // Changed from SPIFFS.exists
        save();
        return;
    }

    File file = LittleFS.open("/config.json", "r"); // Changed from SPIFFS.open
    if (!file) {
        Serial.println("ERROR: Failed to open config.json for reading!");
        return;
    }
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
        Serial.print("ERROR: Failed to parse config.json: ");
        Serial.println(error.c_str());
    }
    file.close();

    data.beep_enabled = doc["beep"] | true;
    data.write_empty_only = doc["write_empty"] | true;
    data.clone_serial = doc["clone_serial"] | false;
    data.brightness = doc["brightness"] | 255;
    data.printer_ip = doc["printer_ip"] | "192.168.1.100";
}

void ConfigManager::save() {
    JsonDocument doc;
    doc["beep"] = data.beep_enabled;
    doc["write_empty"] = data.write_empty_only;
    doc["clone_serial"] = data.clone_serial;
    doc["brightness"] = data.brightness;
    doc["printer_ip"] = data.printer_ip;

    File file = LittleFS.open("/config.json", "w"); // Changed from SPIFFS.open
    if (!file) {
        Serial.println("ERROR: Failed to open config.json for writing!");
        return;
    }
    serializeJson(doc, file);
    file.close();
}