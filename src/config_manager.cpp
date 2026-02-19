#include <config_manager.h>
#include <LittleFS.h>
#include <atomic_write.h>

ConfigManager config;

ConfigManager::ConfigManager() {}

void ConfigManager::init() {
    if(!LittleFS.begin(true)) {
        Serial.println("WARNING: LittleFS failed to mount for ConfigManager!");
        return;
    }
    recoverTmpFile("/config.json");
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

    // P1.10: DB update metadata
    data.db_hash = doc["db_hash"] | "";
    data.db_updated_at = doc["db_updated_at"] | 0;
    data.db_profile_count = doc["db_profile_count"] | 0;
    data.db_schema_version = doc["db_schema_version"] | 0;
    data.printer_model = doc["printer_model"] | "";
}

void ConfigManager::save() {
    JsonDocument doc;
    doc["beep"] = data.beep_enabled;
    doc["write_empty"] = data.write_empty_only;
    doc["clone_serial"] = data.clone_serial;
    doc["brightness"] = data.brightness;
    doc["printer_ip"] = data.printer_ip;

    // P1.10: DB update metadata
    doc["db_hash"] = data.db_hash;
    doc["db_updated_at"] = data.db_updated_at;
    doc["db_profile_count"] = data.db_profile_count;
    doc["db_schema_version"] = data.db_schema_version;
    doc["printer_model"] = data.printer_model;

    if (!atomicWriteJson("/config.json", doc)) {
        Serial.println("ERROR: Failed to save config.json atomically!");
    }
}