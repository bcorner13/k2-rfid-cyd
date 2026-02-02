#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

struct AppConfig {
    bool beep_enabled = true;
    bool write_empty_only = true;
    bool clone_serial = false;
    uint8_t brightness = 255;
    String printer_ip = "192.168.1.100"; // Default
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