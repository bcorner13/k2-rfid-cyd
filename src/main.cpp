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
#include "board_pins.h"

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("--- Booting ---");

    // Initialize WDT early to handle boot load
    esp_task_wdt_init(10, true); 
    esp_task_wdt_add(NULL);

    // 1. Initialize display and show splash
    lvgl_display_init();
    Serial.println("Splash Screen Initialized");

    // --- Initialize Sound ---
    // V1.3: passive buzzer on Mabee GPIO1 (GPIO19). V2.0: I2S on GPIO2/19/20.
    // Uncomment once audio hardware is connected and board version confirmed.
    // uiSound.init();
    // uiSound.playStartup();

    // 2. Add status labels
    splash_add_status("WiFi", false);
    splash_add_status("RFID", false);
    splash_add_status("Bluetooth", false);
    splash_add_status("Filament DB", false);

    // 3. Initialize modules and update status
    network.init();
    bool wifi_triggered = network.connect(); // Try to connect with saved creds
    splash_update_status(0, wifi_triggered ? "Connecting..." : "No Saved WiFi", false);
    
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);  // Makerfabs MaTouch shared I2C bus (GT911 + Mabee/PN532)

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
    splash_update_status(1, rfid_ok ? "Available" : "Not Found", rfid_ok);

    // For now, let's assume Bluetooth is available if the code compiles
    splash_update_status(2, "Available", true);
    bool db_ok = filamentDB.init();
    splash_update_status(3, db_ok ? "Loaded" : "Failed", db_ok);

    // 4. Force a 10-second pause to show final connection status
    Serial.println("Booting: holding splash screen for 10 seconds...");
    uint32_t wait_start = millis();
    while (millis() - wait_start < 10000) {
        esp_task_wdt_reset();
        
        // Keep updating WiFi status in case it connects during the 10s
        static bool last_wifi_state = false;
        bool wifi_now = network.isConnected();
        if (wifi_now != last_wifi_state) {
            if (wifi_now) {
                String ssid = WiFi.SSID();
                splash_update_status(0, ssid.length() > 0 ? ssid.c_str() : "Connected", true);
            } else {
                splash_update_status(0, "Connecting...", false);
            }
            last_wifi_state = wifi_now;
        }
        
        lv_timer_handler();
        delay(20);
    }

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

// --- Background RFID Task ---
void rfid_task() {
    static uint32_t last_poll = 0;
    static String last_uid = "";
    static uint32_t last_detection_time = 0;
    static bool last_read_failed = false;  // require tag removal before retry after failure

    // Only auto-poll in IDLE state
    if (sysState.getCurrentState() != SystemState::IDLE) return;

    // Poll every 500ms
    if (millis() - last_poll < 500) return;
    last_poll = millis();

    if (rfid.checkTagPresent()) {
        String current_uid = rfid.formatUID();

        // After a failed read: require tag removal before retrying the same tag
        if (last_read_failed && current_uid == last_uid) return;

        // Debounce: Only trigger if it's a NEW tag or it's been 3 seconds since last read
        if (current_uid != last_uid || (millis() - last_detection_time > 3000)) {
            Serial.printf("RFID: Auto-detected Tag %s\n", current_uid.c_str());

            sysState.handleEvent(SystemEvent::READ_REQUEST);
            ui.updateButtonStates();
            ui.screenMain.setWriteStatus("Auto-Reading...");

            SpoolData readSpool;
            if (rfid.readCFSTag(readSpool)) {
                Serial.println("RFID: Auto-Read Success");
                ui.updateDashboardFromSpool(readSpool);
                ui.screenMain.setWriteStatus("Tag Read OK", true, false);
                feedback.readSuccess();
                sysState.handleEvent(SystemEvent::OPERATION_SUCCESS);
                last_read_failed = false;
            } else {
                Serial.println("RFID: Auto-Read failed (Auth/CRC)");
                ui.screenMain.setWriteStatus("Read failed", false, false);
                feedback.operationFailed();
                sysState.handleEvent(SystemEvent::OPERATION_FAILED);
                last_read_failed = true;
            }

            last_uid = current_uid;
            last_detection_time = millis();
            ui.updateButtonStates();
        }
    } else {
        // Tag removed — clear debounce and allow retry on next placement
        if (!last_uid.isEmpty()) {
            Serial.println("RFID: Tag removed");
            last_uid = "";
            last_read_failed = false;
        }
    }
}

void loop() {
    esp_task_wdt_reset();
    static uint32_t last_ms = millis();
    static uint32_t last_mem_ms = 0;
    uint32_t now = millis();

    // Log memory every 10 seconds
    if (now - last_mem_ms > 10000) {
        Serial.printf("Heap: Free=%u, MinFree=%u, InternalFree=%u\n", 
                      ESP.getFreeHeap(), ESP.getMinFreeHeap(), 
                      heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
        last_mem_ms = now;
    }

    lv_tick_inc(now - last_ms);
    last_ms = now;
    lv_timer_handler();
    network.process();
    rfid_task(); // 🟢 Run the auto-detection task

    // If we are in WiFi Config state and just connected, wrap up and return to settings
    if (sysState.getCurrentState() == SystemState::WIFI_CONFIG && network.isConnected()) {
        Serial.println("WiFi Connected! Exiting config portal...");
        esp_task_wdt_add(NULL); // Re-enable watchdog
        sysState.handleEvent(SystemEvent::OPERATION_SUCCESS);
        ui.updateButtonStates(); // 🟢 Un-ghost the buttons
        ui.showSettingsScreen();
    }

    feedback.update();

    // ERROR state auto-dismiss: tick() fires TIMEOUT after auto_dismiss_ms
    if (sysState.tick()) {
        ui.screenMain.setWriteStatus("Ready");
        ui.updateButtonStates();
    }

    delay(5);
}
