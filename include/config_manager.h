#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

struct AppConfig {
    bool beep_enabled = true;
    bool write_empty_only = true;
    bool clone_serial = false;
    String printer_ip = "192.168.1.100";

    // P1.10: Database update metadata (FSD Section 5.6)
    String db_hash;                 // SHA-256 hex of last downloaded DB
    uint32_t db_updated_at = 0;     // Uptime seconds when last updated
    uint16_t db_profile_count = 0;  // Number of profiles in last DB
    uint8_t db_schema_version = 0;  // 1 = known format, 0 = unknown/never updated
    String printer_model = "F008";  // Selects /downloads/defData/material/{model}_material_database.json. Default = K2 Plus (F008). Empty = canonical (no prefix).
};

class ConfigManager {
public:
    ConfigManager();
    void init();
    void save();

    AppConfig data;

private:
    void load();
};

extern ConfigManager config;