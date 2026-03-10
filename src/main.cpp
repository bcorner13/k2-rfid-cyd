/**
 * @file main.cpp
 * @brief Application entry: display init, filament DB, config, UI.
 *
 * Boot sequence: serial, LVGL display (splash), optional WiFi/RFID status,
 * LittleFS + FilamentDB load, then UIManager and main loop (lv_timer_handler).
 */
#include <Arduino.h>
#include <esp_task_wdt.h>
#include <ui/screens/screen_filament_select.h>
#include <lvgl_display.h>
#include <ui/ui_manager.h>
#include <system_state.h>
#include <config_manager.h>
#include <inventory_manager.h>
#include <filament_db.h>
#include <network_manager.h>
#include <rfid_driver.h>
#include <Wire.h>
#include <feedback.h>
#include <ui/screens/screen_about.h>

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("--- Booting ---");

    // 1. Initialize display and show splash
    lvgl_display_init();
    Serial.println("Splash Screen Initialized");

    // --- Initialize Sound ---
    // V1.3: passive buzzer on Mabee GPIO1 (GPIO19). V2.0: I2S on GPIO2/19/20.
    // Uncomment once audio hardware is connected and board version confirmed.
    // uiSound.init();
    // uiSound.playStartup();

    // 2. Add status labels (these are commented out, so no labels are added)
    // splash_add_status("WiFi", false);
    // splash_add_status("RFID", false);
    // splash_add_status("Bluetooth", false);
    // splash_add_status("Filament DB", false);

    // 3. Initialize modules and update status
    // network.init(); // Temporarily commented out for debugging
    bool wifi_ok = false; // Default to false when network is commented out
    // bool wifi_ok = WiFi.isConnected(); // Temporarily commented out for debugging
    // splash_update_status(0, wifi_ok ? WiFi.SSID().c_str() : "Not Connected", wifi_ok); // Commented out

    Wire.begin(17, 18);  // SDA=GPIO17, SCL=GPIO18 — Makerfabs MaTouch shared I2C bus (GT911 + Mabee/PN532)

    // I2C bus scan — shows all responding device addresses
    Serial.println("I2C scan:");
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0)
            Serial.printf("  found 0x%02X\n", addr);
    }
    Serial.println("I2C scan done");

    rfid.init();
    uint32_t rfid_ver = rfid.getFirmwareVersion();
    bool rfid_ok = rfid_ver > 0;
    Serial.printf("RFID: %s (ver=0x%08X)\n", rfid_ok ? "OK" : "NOT FOUND", rfid_ver);
    // splash_update_status(1, rfid_ok ? "Available" : "Not Found", rfid_ok); // Commented out

    // For now, let's assume Bluetooth is available if the code compiles
    // splash_update_status(2, "Available", true); // Commented out

    bool db_ok = filamentDB.init();
    // splash_update_status(3, db_ok ? "Loaded" : "Failed", db_ok); // Commented out

    // 4. Final delay to show status, then start UI
    delay(2500);

    lvgl_display_start_ui();
    Serial.println("LVGL Display Initialized for UI");

    sysState.init();
    config.init();
    inventory.init();
    feedback.init();

    ui.init();
    // screenAbout is initialized inside UIManager::init() — do not double-init
    screenFilamentSelect.init();
    Serial.println("UI Initialized");
}

void loop() {
    esp_task_wdt_reset();
    static uint32_t last_ms = millis();
    uint32_t now = millis();
    lv_tick_inc(now - last_ms);   // 🔴 THIS WAS MISSING
    last_ms = now;
    lv_timer_handler();
    feedback.update();
    delay(5);
}
