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
#include <spoolman_client.h>
#include <sd_storage.h>
#include <spoolman_sync.h>
#include <Wire.h>
#include <feedback.h>
#include <ui/ui_sound.h>
#include <ui/screens/screen_about.h>
#include "board_pins.h"

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("--- Booting ---");
    Serial.printf("Reset reason: %d (1 power-on, 3 software, 4 panic, 5 int-wdt, 6 task-wdt, 7 wdt, 9 brownout)\n",
                  (int)esp_reset_reason());
    Serial.printf("Mem[boot]:    heap=%u  psram=%u  minHeap=%u\n",
                  ESP.getFreeHeap(), ESP.getFreePsram(), ESP.getMinFreeHeap());

    // Initialize WDT early to handle boot load (increased to 20s)
    esp_task_wdt_init(20, true);
    esp_task_wdt_add(NULL);

    // 1. Initialize display and show splash
    lvgl_display_init();
    Serial.println("Splash Screen Initialized");
    Serial.printf("Mem[display]: heap=%u  psram=%u\n", ESP.getFreeHeap(), ESP.getFreePsram());

    // --- Initialize Sound ---
    uiSound.init();
    uiSound.playStartup();

    // 2. Add status labels
    splash_add_status("WiFi", false);
    splash_add_status("RFID", false);
    splash_add_status("Bluetooth", false);
    splash_add_status("Filament DB", false);

    // 3. Initialize modules and update status
    esp_task_wdt_reset();
    network.init();
    bool wifi_triggered = network.connect(); // Try to connect with saved creds
    splash_update_status(0, wifi_triggered ? "Connecting..." : "No Saved WiFi", false);
    
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);  // Makerfabs MaTouch shared I2C bus (GT911 + Mabee/PN532)
    esp_task_wdt_reset();

    // I2C bus scan — shows all responding device addresses
    Serial.println("I2C scan:");
    for (uint8_t addr = 1; addr < 127; addr++) {
        if (addr % 16 == 0) esp_task_wdt_reset();
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0)
            Serial.printf("  found 0x%02X\n", addr);
    }
    Serial.println("I2C scan done");
    esp_task_wdt_reset();

    rfid.init();
    uint32_t rfid_ver = rfid.getFirmwareVersion();
    bool rfid_ok = rfid_ver > 0;
    Serial.printf("RFID: %s (ver=0x%08X)\n", rfid_ok ? "OK" : "NOT FOUND", rfid_ver);
    Serial.printf("Mem[rfid]:    heap=%u  psram=%u\n", ESP.getFreeHeap(), ESP.getFreePsram());
    splash_update_status(1, rfid_ok ? "Available" : "Not Found", rfid_ok);
    esp_task_wdt_reset();

    // For now, let's assume Bluetooth is available if the code compiles
    splash_update_status(2, "Available", true);
    sdStorage.mount();      // before the DB load: the material DB is read from the card if it has one
    bool db_ok = filamentDB.init();
    Serial.printf("Mem[filamentDB]: heap=%u  psram=%u\n", ESP.getFreeHeap(), ESP.getFreePsram());
    splash_update_status(3, db_ok ? "Loaded" : "Failed", db_ok);
    esp_task_wdt_reset();

    // Show Continue button — all status items are now populated.
    splash_show_continue_button();

    // 4. Hold splash up to 10 seconds; exit early if user taps Continue.
    Serial.println("Booting: holding splash screen (max 10s)...");
    uint32_t wait_start = millis();
    while (millis() - wait_start < 10000) {
        esp_task_wdt_reset();

        if (splash_is_continue_pressed()) {
            Serial.println("Splash: Continue tapped");
            break;
        }

        // Keep updating WiFi status in case it connects during the wait
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
    esp_task_wdt_reset();

    sysState.init();
    config.init();
    spoolmanClient.init();
    Serial.printf("Mem[spoolman]:   heap=%u  psram=%u\n", ESP.getFreeHeap(), ESP.getFreePsram());
    sdStorage.begin();      // no card or a bad card only turns offline mode off
    spoolmanSync.init();
    Serial.printf("Mem[sync]:       heap=%u  psram=%u\n", ESP.getFreeHeap(), ESP.getFreePsram());
    inventory.init();
    Serial.printf("Mem[inventory]:  heap=%u  psram=%u\n", ESP.getFreeHeap(), ESP.getFreePsram());
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
    static uint8_t miss_count = 0;         // consecutive polls with no tag

    // Only auto-poll in IDLE state, and not while the overwrite dialog is open.
    // Both skips used to be silent, so a stuck state looked like a dead reader (2026-11-09).
    static uint32_t last_blocked_log = 0;
    bool notIdle = sysState.getCurrentState() != SystemState::IDLE;
    if (notIdle || ui.writeConfirmPending) {
        if (millis() - last_blocked_log > 5000) {
            Serial.printf("RFID: polling paused (%s)\n",
                          notIdle ? sysState.getStateName() : "overwrite dialog open");
            last_blocked_log = millis();
        }
        return;
    }

    // Poll every 500ms
    if (millis() - last_poll < 500) return;
    last_poll = millis();

    // No tag for 10 s: make sure the PN532 is still alive. A wedged chip looks exactly like
    // "no tag" forever (seen 2026-11-09 after repeated failed auths during an antenna problem).
    static uint32_t last_seen_or_checked = millis();
    if (millis() - last_seen_or_checked > 10000) {
        rfid.healthCheck();
        last_seen_or_checked = millis();
    }

    if (rfid.checkTagPresent()) {
        miss_count = 0;
        last_seen_or_checked = millis();
        String current_uid = rfid.formatUID();

        // Armed write: do it now, in the session that just selected the card.
        if (ui.writeArmed) {
            Serial.printf("RFID: Tag %s detected for write\n", current_uid.c_str());
            ui.onTagForWrite();
            last_uid = current_uid;
            last_detection_time = millis();
            last_read_failed = true;   // don't auto-read it until it's lifted and replaced
            return;
        }

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
                ui.updateDashboardFromSpool(readSpool, true);
                ui.spoolmanOnTagRead(current_uid);   // queued; rfid_task never waits on the network
                ui.screenRfidRaw.populateTable(readSpool.getRawData().c_str()); // Populate Raw Log
                ui.screenMain.setWriteStatus("Tag Read OK", true, false);
                feedback.readSuccess();
                sysState.handleEvent(SystemEvent::OPERATION_SUCCESS);
                last_read_failed = false;
            } else if (rfid.lastReadWasBlank()) {
                // Blank tag is a normal state, not an error — don't lock the UI.
                Serial.println("RFID: Blank tag");
                ui.screenMain.setWriteStatus("Blank tag - ready to write", true, true);
                sysState.handleEvent(SystemEvent::OPERATION_SUCCESS);
                last_read_failed = true;   // don't re-read it every 3 s
            } else {
                Serial.println("RFID: Auto-Read failed (Auth/CRC) - retrying silently");
                ui.screenMain.setWriteStatus("Reading...", false, false);
                // feedback.operationFailed(); // Removed to prevent annoying double-beeps on placement
                sysState.handleEvent(SystemEvent::OPERATION_FAILED);
                last_read_failed = true;
            }

            last_uid = current_uid;
            last_detection_time = millis();
            ui.updateButtonStates();
        }
    } else if (miss_count < 255 && ++miss_count >= 3) {
        // Tag removed — require 3 consecutive misses (~1.5 s); a single miss right
        // after a failed auth was falsely reported as removal (cfs-tag-write-design.md bug C).
        if (!last_uid.isEmpty()) {
            Serial.println("RFID: Tag removed");
            spoolmanClient.tagRemoved();
            last_uid = "";
            last_read_failed = false;
        }
    }
}

// Serial console for bench testing: "url" prints the Spoolman URL, "url <value>" sets and saves it
// (same as the Wi-Fi portal field), "sync" starts a sync now, "colors <id>" lists a profile's colours.
static void serial_console() {
    static char line[160];
    static size_t len = 0;
    while (Serial.available()) {
        const char c = (char)Serial.read();
        if (c != '\n' && c != '\r') {
            if (len < sizeof(line) - 1) line[len++] = c;
            continue;
        }
        if (len == 0) continue;
        line[len] = '\0';
        len = 0;
        if (strcmp(line, "url") == 0) {
            Serial.printf("CONSOLE: spoolman_url=\"%s\"\n", config.data.spoolman_url.c_str());
        } else if (strncmp(line, "url ", 4) == 0) {
            config.data.spoolman_url = String(line + 4);
            config.data.spoolman_url.trim();
            config.save();
            spoolmanSync.setBaseUrl(config.data.spoolman_url.c_str());
            spoolmanSync.kick();
            Serial.printf("CONSOLE: spoolman_url set to \"%s\"\n", config.data.spoolman_url.c_str());
        } else if (strcmp(line, "printer") == 0) {
            Serial.printf("CONSOLE: printer_ip=\"%s\"\n", config.data.printer_ip.c_str());
        } else if (strncmp(line, "printer ", 8) == 0) {   // same as the portal's "Printer IP" field
            config.data.printer_ip = String(line + 8);
            config.data.printer_ip.trim();
            config.save();
            Serial.printf("CONSOLE: printer_ip set to \"%s\"\n", config.data.printer_ip.c_str());
        } else if (strcmp(line, "state") == 0) {
            TaskHandle_t t = xTaskGetHandle("spoolsync");
            Serial.printf("CONSOLE: sync state=%d lastPull=%lld sd=%d (%s) task=%d stackFree=%u\n",
                          (int)spoolmanSync.state(), (long long)spoolmanSync.lastPullEpoch(),
                          (int)sdStorage.available(), sdStorage.failure(), t ? (int)eTaskGetState(t) : -1,
                          t ? (unsigned)uxTaskGetStackHighWaterMark(t) : 0);
        } else if (strncmp(line, "colors ", 7) == 0) {   // read-only: a profile's parsed colours
            FilamentProfile p;
            if (!filamentDB.getProfileById(String(line + 7), p)) {
                Serial.printf("CONSOLE: no profile '%s'\n", line + 7);
            } else {
                Serial.printf("CONSOLE: %s %s: %u colours\n", p.id.c_str(), p.name.c_str(), (unsigned)p.colors.size());
                for (size_t i = 0; i < p.colors.size(); i++)
                    Serial.printf("CONSOLE:   #%06X a=%02X \"%s\"\n", (unsigned)p.colors[i],
                                  i < p.color_alphas.size() ? p.color_alphas[i] : 0xFF,
                                  i < p.color_names.size() ? p.color_names[i].c_str() : "");
            }
        } else if (strcmp(line, "sync") == 0) {
            spoolmanSync.kick();
            Serial.println("CONSOLE: sync started");
        } else {
            Serial.printf("CONSOLE: unknown command '%s' (url | url <value> | printer | printer <ip> | colors <id> | sync | state)\n", line);
        }
    }
}

void loop() {
    esp_task_wdt_reset();
    serial_console();
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
    ui.spoolmanTick();

    // Update status bar (battery placeholder + WiFi) every 2 seconds
    static uint32_t last_status_ms = 0;
    if (now - last_status_ms >= 2000) {
        bool wifiOk = network.isConnected();
        int8_t rssi  = wifiOk ? (int8_t)WiFi.RSSI() : 0;
        ui.updateStatusBar(wifiOk, rssi, wifiOk ? WiFi.SSID().c_str() : nullptr);
        last_status_ms = now;
    }

    // If we are in WiFi Config state and just connected, wrap up and return to settings
    if (sysState.getCurrentState() == SystemState::WIFI_CONFIG && network.isConnected()) {
        Serial.println("WiFi Connected! Exiting config portal...");
        esp_task_wdt_add(NULL); // Re-enable watchdog
        sysState.handleEvent(SystemEvent::OPERATION_SUCCESS);
        ui.updateButtonStates(); // 🟢 Un-ghost the buttons
        ui.showSettingsScreen();
    }

    feedback.update();
    ui.writeTick();

    // ERROR state auto-dismiss: tick() fires TIMEOUT after auto_dismiss_ms
    if (sysState.tick()) {
        if (!ui.writeArmed) ui.screenMain.setWriteStatus("Ready");
        ui.updateButtonStates();
    }

    delay(5);
}
