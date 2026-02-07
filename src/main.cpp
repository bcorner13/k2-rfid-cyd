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
#include <filament_db.h>
#include <network_manager.h>
#include <ui/screens/screen_about.h>

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("--- Booting Full Application ---");

    // 1. Initialize display and show splash
    lvgl_display_init();
    Serial.println("Splash Screen Initialized");

    // --- Initialize Sound (Sensor AD Pin = GPIO 11) ---
    // uiSound.init(11); // Commented out as sound is tabled
    // uiSound.playStartup(); // Play a startup chime // Commented out as sound is tabled

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

    // rfid.init(); // Temporarily commented out for debugging
    bool rfid_ok = false; // Default to false when rfid is commented out
    // bool rfid_ok = rfid.getFirmwareVersion() > 0; // Temporarily commented out for debugging
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

    ui.init();
    screenAbout.init(); // Initialize the About screen widgets
    screenFilamentSelect.init(); // Initialize the Filament Select screen widgets
    Serial.println("UI Initialized");
}

void loop() {
    esp_task_wdt_reset();
    static uint32_t last_ms = millis();
    uint32_t now = millis();
    lv_tick_inc(now - last_ms);   // 🔴 THIS WAS MISSING
    last_ms = now;
    lv_timer_handler();
    delay(5);
}
