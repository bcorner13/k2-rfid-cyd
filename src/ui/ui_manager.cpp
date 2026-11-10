#include <color_names.h>
#include <esp_task_wdt.h>
#include <ui/ui_manager.h>
#include <ui/color_palette.h>
#include <filament_db.h>
#include <config_manager.h>
#include <network_manager.h>
#include <rfid_driver.h>
#include <spoolman_client.h>
#include <sd_storage.h>
#include <spoolman_sync.h>
#include <sync_status_text.h>
#include <time.h>
#include <system_state.h>
#include <inventory_manager.h>
#include <feedback.h>

// Include screen headers from the new include path
#include <ui/screens/screen_main.h>
#include <ui/screens/screen_library.h>
#include <ui/screens/screen_settings.h>
#include <ui/screens/screen_about.h>
#include <ui/screens/screen_inventory.h>
#include <ui/screens/screen_spool_detail.h>
#include <ui/screens/screen_custom_entry.h>
#include <ui/screens/screen_wifi.h>

UIManager ui;

// --- CFS tag write (docs/rfid/cfs-tag-write-design.md) ---

static constexpr uint32_t WRITE_ARM_TIMEOUT_MS = 30000;
static bool     s_writeAllowOverwrite = false;
static uint32_t s_writeArmedMs = 0;

static void overwrite_confirm_cb(lv_event_t* e) {
    ui.writeConfirmPending = false;
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    bool confirmed = (bool)(uintptr_t)lv_event_get_user_data(e);
    lv_msgbox_close(lv_obj_get_parent(lv_obj_get_parent(btn)));   // button -> footer -> msgbox
    if (confirmed) ui.armWrite(true);
    else ui.cancelWrite("Write cancelled");
}

void UIManager::armWrite(bool allowOverwrite) {
    if (currentSpool.getMaterialId().length() != 5) {
        screenMain.setWriteStatus("Pick a filament first", false, false);
        return;
    }
    writeArmed = true;
    s_writeAllowOverwrite = allowOverwrite;
    s_writeArmedMs = millis();
    screenMain.setWriteStatus(allowOverwrite ? "Lift and place the tag again" : "Place tag to write...", true, true);
}

void UIManager::cancelWrite(const char* why) {
    writeArmed = false;
    screenMain.setWriteStatus(why);
}

void UIManager::writeTick() {
    if (writeArmed && millis() - s_writeArmedMs > WRITE_ARM_TIMEOUT_MS) cancelWrite("Write cancelled (timeout)");
}

void UIManager::selectMaterial(const char* brand, const char* type, bool brandChanged, const char* product) {
    const FilamentProfile* match = nullptr;      // brand + type
    const FilamentProfile* brandAny = nullptr;   // brand, any type
    const FilamentProfile* generic = nullptr;    // Generic + type
    for (const auto& p : filamentDB.getCache()) {
        if (p.brand == brand) {
            if (!brandAny) brandAny = &p;
            if (!match && p.material_type == type && (!product || p.name == product)) match = &p;
        }
        if (!generic && p.brand == "Generic" && p.material_type == type) generic = &p;
    }
    const FilamentProfile* pick = match ? match : (brandChanged ? brandAny : generic);
    if (!pick) {
        screenMain.setWriteStatus("No profile for that material", false, false);
        return;
    }
    SpoolData sd(*pick);
    sd.setColor(currentSpool.getColorHex());      // keep the user's color and weight
    sd.setWeight(currentSpool.getWeight());
    updateDashboardFromSpool(sd);
    char msg[64];
    if (match) snprintf(msg, sizeof(msg), "Selected %s", pick->name.c_str());
    else       snprintf(msg, sizeof(msg), "No %s %s in DB - using %s", brand, type, pick->name.c_str());
    screenMain.setWriteStatus(msg, match != nullptr, match != nullptr);
}

void UIManager::onTagForWrite() {
    writeArmed = false;
    clearSpoolmanInfo();   // the write changes the tag, so its old record no longer applies
    sysState.handleEvent(SystemEvent::WRITE_REQUEST);
    updateButtonStates();
    screenMain.setWriteStatus("Writing...");
    lv_refr_now(NULL);   // the write blocks the UI loop; show the status first

    SpoolData existing;
    CfsWriteResult r = rfid.writeCFSTag(currentSpool, s_writeAllowOverwrite, &existing);
    Serial.printf("UI: CFS write -> %s\n", cfsWriteResultText(r));

    if (r == CfsWriteResult::OK) {
        screenMain.setWriteStatus(cfsWriteResultText(r), true, false);
        sysState.handleEvent(SystemEvent::OPERATION_SUCCESS);   // WRITING -> VERIFYING
        sysState.handleEvent(SystemEvent::OPERATION_SUCCESS);   // VERIFYING -> IDLE (verified inside writeCFSTag)
        feedback.writeSuccess();
    } else if (r == CfsWriteResult::NEEDS_CONFIRM) {
        sysState.handleEvent(SystemEvent::USER_DISMISS);        // WRITING -> IDLE; nothing written
        screenMain.setWriteStatus(cfsWriteResultText(r), false, true);
        // e.g. "Creality CR-Silk\nColor #000000  -  1000 g"
        const std::string name = existing.getDisplayName().empty()
            ? ("Material " + existing.getMaterialId()) : (existing.getBrand() + " " + existing.getDisplayName());
        String cname = filamentDB.productColorName(existing.getMaterialId().c_str(), existing.getColorHex());
        if (cname.isEmpty()) cname = String("~") + nearestColorName(existing.getColorHex());
        char msg[192];
        snprintf(msg, sizeof(msg), "This tag already holds:\n\n%s\n%s (%s)  -  %u g\n\nReplace it with the selected filament?",
                 name.c_str(), cname.c_str(), existing.getColorName().c_str(), existing.getWeight());
        writeConfirmPending = true;
        lv_obj_t* mbox = lv_msgbox_create(NULL);
        lv_msgbox_add_title(mbox, "Overwrite tag?");
        lv_msgbox_add_text(mbox, msg);
        lv_obj_t* yes = lv_msgbox_add_footer_button(mbox, "Overwrite");
        lv_obj_t* no  = lv_msgbox_add_footer_button(mbox, "Cancel");
        lv_obj_add_event_cb(yes, overwrite_confirm_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)true);
        lv_obj_add_event_cb(no,  overwrite_confirm_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)false);
    } else {
        screenMain.setWriteStatus(cfsWriteResultText(r), false, false);
        sysState.handleEvent(SystemEvent::OPERATION_FAILED);
        feedback.operationFailed();
    }
    updateButtonStates();
}

UIManager::UIManager() {

}

void UIManager::init() {
    // Force Light Theme
    lv_display_t * disp = lv_display_get_default();
    // lv_theme_t * theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), false, LV_FONT_DEFAULT);
    lv_theme_t * theme = lv_theme_default_init(
        disp,
        lv_color_make(0x21, 0x96, 0xF3),
        lv_color_make(0xF4, 0x43, 0x36),
        false,
        LV_FONT_DEFAULT
        ); // Replaced lv_palette_main with lv_color_make
    lv_display_set_theme(disp, theme);

    // Disable watchdog during UI init — creating screens with many LVGL
    // objects can exceed the default WDT timeout.
    esp_task_wdt_delete(NULL);

    screenMain.init();
    screenLibrary.init();
    screenSettings.init();
    screenAbout.init();
    screenInventory.init();
    screenSpoolDetail.init();   // deferred — actual creation on first show()
    screenCustomEntry.init();   // deferred — actual creation on first show()
    screenWifi.init();
    screenRfidRaw.init();

    // Register event handlers once (not on every screen transition)
    lv_obj_add_event_cb(screenMain.labelWriteStatus, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenMain.btnSettings, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenMain.btnLibrary, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenMain.btnLibrary, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenMain.btnInventory, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenMain.btnWrite, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenMain.colorBlock, event_handler, LV_EVENT_CLICKED, NULL);   // dropped in fb35184
    lv_obj_add_event_cb(screenMain.btnmWeight, event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(screenMain.ddBrand, event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(screenMain.ddType, event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(screenMain.ddProduct, event_handler, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_add_event_cb(screenSettings.btnBack, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenSettings.btnAbout, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenSettings.swBeep, event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(screenSettings.btnUpdateDB, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenSettings.btnSetupWifi, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenSettings.btnResetWifi, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenSettings.btnRestart, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenSettings.btnRfidRaw, event_handler, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(screenAbout.btnBack, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenRfidRaw.btnBack, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenLibrary.btnBack, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenInventory.btnBack, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenWifi.btnBack, event_handler, LV_EVENT_CLICKED, NULL);

    // Spool detail + custom entry event handlers registered lazily in
    // showSpoolDetail() / showCustomEntry() on first navigation.

    createColorPicker();
    createOverlay();

    // Default to the real DB Generic PLA so the dashboard carries a valid material ID
    // ("00001"); the old placeholder had id "0" and every Write failed with no material.
    FilamentProfile defaultProfile;
    if (filamentDB.getProfileById("00001", defaultProfile)) {
        updateDashboardFromSpool(SpoolData(defaultProfile));
    } else {
        screenMain.setWriteStatus("Pick a filament", false, true);
    }

    showMainScreen();
    updateButtonStates(); // 🟢 Ensure icons are active on boot

    // Re-enable watchdog for normal operation
    esp_task_wdt_add(NULL);
    Serial.println("UI init complete");
}

void UIManager::update() {
    lv_timer_handler();
}

// ---------------------------------------------------------------------------
// P1.9: Operation lock helpers (FSD Section 12.5)
// ---------------------------------------------------------------------------
bool UIManager::isSystemBusy() {
    return sysState.isBusy();
}

void UIManager::updateButtonStates() {
    bool busy = isSystemBusy();

    // Helper lambdas for enable/disable
    auto setEnabled = [](lv_obj_t* obj, bool enabled) {
        if (!obj) return;
        if (enabled)
            lv_obj_clear_state(obj, LV_STATE_DISABLED);
        else
            lv_obj_add_state(obj, LV_STATE_DISABLED);
    };

    // Main screen: operation buttons disabled when busy
    setEnabled(screenMain.btnInventory, !busy);
    setEnabled(screenMain.btnWrite, !busy);
    setEnabled(screenMain.btnLibrary, !busy);
    setEnabled(screenMain.colorBlock, !busy);
    setEnabled(screenMain.btnmWeight, !busy);
    setEnabled(screenMain.ddProduct, !busy);
    // Settings button always active (navigation only)

    // Settings screen: operation buttons disabled when busy
    setEnabled(screenSettings.btnUpdateDB, !busy);
    setEnabled(screenSettings.btnSetupWifi, !busy);
    setEnabled(screenSettings.btnResetWifi, !busy);
    setEnabled(screenSettings.btnRestart, !busy);
    // Back, About, Beep toggle always active

    // Spool detail: Write disabled when busy (only once lazily created)
    if (screenSpoolDetail.btnWriteTag) setEnabled(screenSpoolDetail.btnWriteTag, !busy);
}

// ---------------------------------------------------------------------------
// Event handler with P1.9 busy guard
// ---------------------------------------------------------------------------
void UIManager::event_handler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* obj = (lv_obj_t*) lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED) {
        // Error dismiss: tap status label to return to IDLE (always allowed)
        if (obj == ui.screenMain.labelWriteStatus &&
                sysState.getCurrentState() == SystemState::ERROR) {
            sysState.handleEvent(SystemEvent::USER_DISMISS);
            ui.screenMain.setWriteStatus("Ready");
            ui.updateButtonStates();
            return;
        }

        // Navigation (always allowed regardless of state)
        if (obj == ui.screenMain.btnSettings) { ui.showSettingsScreen(); return; }
        if (obj == ui.screenLibrary.btnBack) { ui.showMainScreen(); return; }
        if (obj == ui.screenSettings.btnBack) { ui.showMainScreen(); return; }
        if (obj == ui.screenSettings.btnAbout) { ui.showAboutScreen(); return; }
        if (obj == ui.screenSettings.btnRfidRaw) { ui.showRfidRawScreen(); return; }
        if (obj == ui.screenAbout.btnBack) { ui.showSettingsScreen(); return; }
        if (obj == ui.screenRfidRaw.btnBack) { ui.showSettingsScreen(); return; }
        if (obj == ui.screenInventory.btnBack) { ui.showMainScreen(); return; }
        if (obj == ui.screenSpoolDetail.btnBack) { ui.showInventoryScreen(); return; }
        if (obj == ui.screenCustomEntry.btnCancel) { ui.showInventoryScreen(); return; }
        if (obj == ui.screenWifi.btnBack) {
            // Re-enable watchdog when leaving setup
            esp_task_wdt_add(NULL);
            
            sysState.handleEvent(SystemEvent::USER_DISMISS);
            ui.showSettingsScreen();
            return;
        }

        // P1.9: Gate all operation-triggering actions when system is busy
        if (isSystemBusy()) {
            Serial.printf("UI: action rejected — system busy (%s)\n", sysState.getStateName());
            return;
        }

        if (obj == ui.screenMain.btnInventory) {
            ui.showInventoryScreen();
            return;
        }

        if (obj == ui.screenMain.btnWrite) {
            Serial.println("UI: Write RFID triggered");
            if (ui.writeArmed) ui.cancelWrite("Write cancelled");   // second press disarms
            else ui.armWrite(false);
        }
        else if (obj == ui.screenMain.btnLibrary) {
            ui.showFilamentLibrary();
        }
        else if (obj == ui.screenSpoolDetail.btnWriteTag) {
            Serial.printf("UI: write tag for spool %d (Task 6)\n", ui.screenSpoolDetail.currentSpoolId);
        }
        else if (obj == ui.screenCustomEntry.btnPrev) {
            if (ui.screenCustomEntry.currentStep > 0)
                ui.screenCustomEntry.goToStep(ui.screenCustomEntry.currentStep - 1);
        }
        else if (obj == ui.screenCustomEntry.btnNext) {
            if (ui.screenCustomEntry.currentStep < 4)
                ui.screenCustomEntry.goToStep(ui.screenCustomEntry.currentStep + 1);
        }
        else if (obj == ui.screenCustomEntry.btnSave ||
                 obj == ui.screenCustomEntry.btnSaveWrite) {
            // Build SpoolRecord from form data
            SpoolRecord rec;
            rec.brand = lv_textarea_get_text(ui.screenCustomEntry.taBrand);
            rec.name  = lv_textarea_get_text(ui.screenCustomEntry.taName);

            char matBuf[8];
            lv_dropdown_get_selected_str(ui.screenCustomEntry.ddMaterial, matBuf, sizeof(matBuf));
            rec.material_type = matBuf;

            char colorBuf[8];
            snprintf(colorBuf, sizeof(colorBuf), "%06X", ui.screenCustomEntry.selectedColor);
            rec.color_hex = colorBuf;

            uint16_t diaIdx = lv_dropdown_get_selected(ui.screenCustomEntry.ddDiameter);
            rec.diameter_um = (diaIdx == 0) ? 1750 : 2850;

            uint32_t weight = lv_spinbox_get_value(ui.screenCustomEntry.spinWeight);
            rec.initial_weight_g = weight;
            rec.current_weight_g = weight;

            rec.nozzle_temp_min = lv_spinbox_get_value(ui.screenCustomEntry.spinNozzleMin);
            rec.nozzle_temp_max = lv_spinbox_get_value(ui.screenCustomEntry.spinNozzleMax);
            rec.bed_temp_min    = lv_spinbox_get_value(ui.screenCustomEntry.spinBedMin);
            rec.bed_temp_max    = lv_spinbox_get_value(ui.screenCustomEntry.spinBedMax);
            rec.print_speed_min = lv_spinbox_get_value(ui.screenCustomEntry.spinSpeedMin);
            rec.print_speed_max = lv_spinbox_get_value(ui.screenCustomEntry.spinSpeedMax);
            rec.fan_percent     = lv_slider_get_value(ui.screenCustomEntry.sliderFan);
            rec.source          = SpoolSource::MANUAL;

            String newId = inventory.addSpoolRecord(std::move(rec));
            if (newId.isEmpty()) {
                Serial.println("ERROR: Failed to add custom spool (inventory full?)");
                return;
            }

            Serial.printf("Custom spool created: %s\n", newId.c_str());
            feedback.spoolSaved();

            ui.showInventoryScreen();   // custom entry is retired with inventory.json (plan Task 10)
        }
        else if (obj == ui.screenMain.colorBlock) {
            ui.showColorPicker();
        }
        else if (obj == ui.screenSettings.btnUpdateDB) {
            sysState.handleEvent(SystemEvent::DB_UPDATE_REQUEST);
            ui.updateButtonStates();

            if (network.updateFilamentDB()) {
                sysState.handleEvent(SystemEvent::DB_UPDATE_SUCCESS);
                feedback.dbUpdateSuccess();
                ui.updateButtonStates();
            } else {
                sysState.handleEvent(SystemEvent::DB_UPDATE_FAILED);
                feedback.dbUpdateFailed();
                // Show error on Main so the user can see it and tap labelWriteStatus to dismiss.
                // showMainScreen() preserves the status label in ERROR state.
                ui.screenMain.setWriteStatus(
                    sysState.getErrorContext().message.c_str(), false, false);
                ui.showMainScreen();
            }
        }
        else if (obj == ui.screenSettings.btnSetupWifi) {
            sysState.handleEvent(SystemEvent::WIFI_CONFIG_REQUEST);
            ui.showWifiScreen();
            
            // Disable watchdog while portal is active to prevent reboots during config
            esp_task_wdt_delete(NULL);
            
            network.startConfigPortal();
        }
        else if (obj == ui.screenSettings.btnResetWifi) {
            lv_label_set_text(lv_obj_get_child(ui.screenSettings.btnResetWifi, 0), "Clearing...");
            lv_timer_handler(); // Force render
            network.reset(); // This now clears settings, waits 1s, and restarts the ESP
        }
        else if (obj == ui.screenSettings.btnRestart) {
            ESP.restart();
        }
        else {
            // Check if tap is on an inventory list row
            lv_obj_t* invRow = obj;
            while (invRow && lv_obj_get_parent(invRow) != ui.screenInventory.list) {
                invRow = lv_obj_get_parent(invRow);
            }
            if (invRow && lv_obj_get_parent(invRow) == ui.screenInventory.list) {
                ui.showSpoolDetail((int)(intptr_t)lv_obj_get_user_data(invRow));
                return;
            }

            /* Filament grid: tap may hit cell, inner, swatch, or label – find the grid cell */
            lv_obj_t* cell = obj;
            while (cell && lv_obj_get_parent(cell) != ui.screenLibrary.grid) {
                cell = lv_obj_get_parent(cell);
            }
            if (cell && lv_obj_get_parent(cell) == ui.screenLibrary.grid) {
                size_t idx = (size_t)lv_obj_get_user_data(cell);
                auto all = filamentDB.getAllFilaments();
                if (idx < all.size()) {
                    SpoolData newSpool(all[idx]);
                    ui.updateDashboardFromSpool(newSpool);
                    ui.showMainScreen();
                }
            }
        }
    }
    else if (code == LV_EVENT_VALUE_CHANGED) {
        if (obj == ui.screenMain.btnmWeight) {
            if (isSystemBusy()) return;  // P1.9: guard size buttons too
            uint32_t idx = lv_buttonmatrix_get_selected_button(obj);
            if (idx >= cfs::kWeightStepCount) return;
            ui.currentSpool.setWeight(cfs::kWeightSteps[idx].grams);
        }
        else if (obj == ui.screenMain.ddBrand || obj == ui.screenMain.ddType) {
            if (isSystemBusy()) return;
            char brand[48], type[24];
            lv_dropdown_get_selected_str(ui.screenMain.ddBrand, brand, sizeof(brand));
            lv_dropdown_get_selected_str(ui.screenMain.ddType, type, sizeof(type));
            ui.selectMaterial(brand, type, obj == ui.screenMain.ddBrand);
        }
        else if (obj == ui.screenMain.ddProduct) {
            if (isSystemBusy()) return;
            char brand[48], type[24], product[96];
            lv_dropdown_get_selected_str(ui.screenMain.ddBrand, brand, sizeof(brand));
            lv_dropdown_get_selected_str(ui.screenMain.ddType, type, sizeof(type));
            lv_dropdown_get_selected_str(ui.screenMain.ddProduct, product, sizeof(product));
            if (strcmp(product, "-") != 0) ui.selectMaterial(brand, type, false, product);
        }
        else if (obj == ui.screenSettings.swBeep) {
            config.data.beep_enabled = lv_obj_has_state(obj, LV_STATE_CHECKED);
            config.save();
        }
    }
}

void UIManager::showMainScreen() {
    updateDashboardFromSpool(currentSpool);
    // Don't clear the status label when in ERROR — preserve the error message
    // so the user can see it and tap to dismiss.
    if (writeArmed) {
        screenMain.setWriteStatus(s_writeAllowOverwrite ? "Lift and place the tag again" : "Place tag to write...", true, true);
    } else if (sysState.getCurrentState() != SystemState::ERROR) {
        screenMain.setWriteStatus("Ready");
    }
    screenMain.show();
    updateButtonStates();  // P1.9: sync button states on screen transition
}

void UIManager::showSettingsScreen() {
    screenSettings.show();
    updateButtonStates();  // P1.9
}

void UIManager::showAboutScreen() {
    screenAbout.show();
}

void UIManager::showRfidRawScreen() {
    screenRfidRaw.show();
}

void UIManager::syncStatusText(char* out, size_t n) {
    formatSyncStatus(sdStorage.available() ? spoolmanSync.state() : SyncState::NoStore,
                     spoolmanSync.lastPullEpoch(), spoolmanSync.lastPullMillis(), millis(),
                     (int64_t)time(nullptr), out, n);
}

void UIManager::showSpoolDetail(int spoolId) {
    static bool eventsRegistered = false;
    SpoolRow row;
    bool found = false;
    {
        StoreGuard g;
        found = g && g->getSpool(spoolId, row);
    }
    if (!found) {
        Serial.printf("UI: spool %d not in local copy (or store busy)\n", spoolId);
        return;
    }
    char status[64];
    syncStatusText(status, sizeof(status));
    screenSpoolDetail.loadSpool(row, status);
    if (!eventsRegistered) {
        eventsRegistered = true;
        lv_obj_add_event_cb(screenSpoolDetail.btnBack, event_handler, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(screenSpoolDetail.btnWriteTag, event_handler, LV_EVENT_CLICKED, NULL);
    }
    screenSpoolDetail.show();
    updateButtonStates();
}

void UIManager::showCustomEntry() {
    static bool eventsRegistered = false;
    screenCustomEntry.reset();
    if (!eventsRegistered) {
        eventsRegistered = true;
        lv_obj_add_event_cb(screenCustomEntry.btnCancel, event_handler, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(screenCustomEntry.btnPrev, event_handler, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(screenCustomEntry.btnNext, event_handler, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(screenCustomEntry.btnSave, event_handler, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(screenCustomEntry.btnSaveWrite, event_handler, LV_EVENT_CLICKED, NULL);
    }
    screenCustomEntry.show();
}

void UIManager::showWifiScreen() {
    screenWifi.show();
    updateButtonStates();
}

void UIManager::refreshInventory() {
    char status[64];
    syncStatusText(status, sizeof(status));
    if (!sdStorage.available()) {
        inventoryRefreshPending = false;
        screenInventory.showMessage("Offline mode unavailable: no SD card.\n"
                                    "Tag read and write work as usual.", status);
        return;
    }
    std::vector<SpoolRow> rows;
    bool ok = false;
    {
        StoreGuard g;   // <= 50 ms; the sync task holds it ~1 s during a pull
        ok = g && g->listSpools(rows, false);
    }
    if (!ok) {
        inventoryRefreshPending = true;   // retried from spoolmanTick()
        screenInventory.showMessage("Updating...", status);
        return;
    }
    inventoryRefreshPending = false;
    inventoryShownCounter = spoolmanSync.changeCounter();
    screenInventory.populate(rows, status);

    // Rows are recreated on every populate, so their tap handlers are too.
    const uint32_t count = lv_obj_get_child_cnt(screenInventory.list);
    for (uint32_t i = 0; i < count; i++) {
        lv_obj_t* row = lv_obj_get_child(screenInventory.list, i);
        if (lv_obj_has_flag(row, LV_OBJ_FLAG_CLICKABLE)) lv_obj_add_event_cb(row, event_handler, LV_EVENT_CLICKED, NULL);
    }
}

void UIManager::showInventoryScreen() {
    refreshInventory();
    screenInventory.show();
    updateButtonStates();
}

void UIManager::showFilamentLibrary() {
    screenLibrary.populate();

    // Grid cells are dynamic (recreated each populate), so register handlers here
    uint32_t count = lv_obj_get_child_cnt(ui.screenLibrary.grid);
    for(uint32_t i=0; i<count; i++) {
        lv_obj_t* btn = lv_obj_get_child(ui.screenLibrary.grid, i);
        lv_obj_add_event_cb(btn, event_handler, LV_EVENT_CLICKED, NULL);
    }

    ui.screenLibrary.show();
}

void UIManager::updateDashboardFromSpool(const SpoolData& data, bool keepSpoolmanInfo) {
    currentSpool = data;
    screenMain.update(data);
    if (!keepSpoolmanInfo) clearSpoolmanInfo();
}

void UIManager::clearSpoolmanInfo() {
    spoolmanClient.clear();
    screenMain.setSpoolmanInfo("");
}

void UIManager::spoolmanOnTagRead(const String& uid) {
    if (spoolmanClient.lookup(uid)) screenMain.setSpoolmanInfo("Spoolman: looking up...");
}

void UIManager::spoolmanTick() {
    if (lv_screen_active() == screenInventory.screen) {
        static uint32_t lastStatusMs = 0;
        if (inventoryRefreshPending || inventoryShownCounter != spoolmanSync.changeCounter()) {
            refreshInventory();
            lastStatusMs = millis();
        } else if (millis() - lastStatusMs > 5000) {   // keep "synced N min ago" / Live-Offline current
            char status[64];
            syncStatusText(status, sizeof(status));
            lv_label_set_text(screenInventory.labelStatus, status);
            lastStatusMs = millis();
        }
    }

    SpoolmanResult res;
    if (!spoolmanClient.poll(res)) return;

    char text[spoolman::SUMMARY_BUF];
    switch (res.state) {
        case SpoolmanState::Found:
            spoolman::formatSummary(res.spool, text, sizeof(text));
            screenMain.setSpoolmanInfo(text, 0x1B5E20);
            break;
        case SpoolmanState::NotFound:
            screenMain.setSpoolmanInfo("Not in Spoolman", 0xB26A00);
            break;
        case SpoolmanState::Offline:
            screenMain.setSpoolmanInfo("Spoolman offline");
            break;
        case SpoolmanState::Error:
            snprintf(text, sizeof(text), "Spoolman error: %s", res.detail);
            screenMain.setSpoolmanInfo(text, 0xC62828);
            break;
    }
}

void UIManager::updateStatusBar(bool wifiConnected, int8_t rssi, const char* ssid) {
    screenMain.updateStatusBar(wifiConnected, rssi, ssid);
}

void UIManager::createOverlay() {
    layerTop = lv_display_get_layer_top(lv_display_get_default());
    lv_obj_clear_flag(layerTop, LV_OBJ_FLAG_SCROLLABLE);  /* prevent touch from scrolling whole screen */
    labelBattery = lv_label_create(layerTop);
    lv_label_set_text(labelBattery, "");
    lv_obj_set_align(labelBattery, LV_ALIGN_TOP_RIGHT);
    lv_obj_set_x(labelBattery, -5);
    lv_obj_set_y(labelBattery, 5);
    lv_obj_add_flag(labelBattery, LV_OBJ_FLAG_HIDDEN);  /* hidden until real battery/voltage */
}

void UIManager::updateBattery(float voltage) {
    if (labelBattery) {
        if (voltage < 1.0) {
            lv_label_set_text(labelBattery, "USB");
        } else {
            lv_label_set_text_fmt(labelBattery, "%.1fV", voltage);
        }
    }
}

// Swatch tap: the color lives in the swatch's user data, not its style (the style gets an outline).
static void onColorSwatch(lv_event_t* e) {
    lv_obj_t* t = (lv_obj_t*)lv_event_get_target(e);
    uint32_t hex = (uint32_t)(uintptr_t)lv_obj_get_user_data(t);
    ui.currentSpool.setColor(hex);
    ui.updateDashboardFromSpool(ui.currentSpool);
    ui.closeColorPicker();
}

static lv_obj_t* makeSwatchRow(lv_obj_t* parent) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_bg_opa(row, 0, 0);
    lv_obj_set_style_pad_all(row, 2, 0);
    lv_obj_set_style_pad_row(row, 8, 0);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    return row;
}

static lv_color_t swatchTextColor(uint32_t hex) {
    const float lum = (0.299f * ((hex >> 16) & 0xFF) + 0.587f * ((hex >> 8) & 0xFF) + 0.114f * (hex & 0xFF)) / 255.0f;
    return lum > 0.5f ? lv_color_black() : lv_color_white();
}

// name != nullptr: a wider swatch with the colour's name inside (product colours).
static void addSwatch(lv_obj_t* row, uint32_t hex, const char* name = nullptr) {
    lv_obj_t* btn = lv_btn_create(row);
    if (name && name[0]) {
        lv_obj_set_size(btn, 104, 56);
        lv_obj_t* l = lv_label_create(btn);
        lv_label_set_text(l, name);
        lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(l, 96);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(l, swatchTextColor(hex), 0);
        lv_obj_center(l);
        lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);   // taps go to the swatch (its user data holds the colour)
    } else {
        lv_obj_set_size(btn, 48, 48);
    }
    lv_obj_set_style_bg_color(btn, lv_color_hex(hex), 0);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_set_style_border_width(btn, 1, 0);                    // keeps white/black visible
    lv_obj_set_style_border_color(btn, lv_color_hex(0x808080), 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_user_data(btn, (void*)(uintptr_t)hex);
    lv_obj_add_event_cb(btn, onColorSwatch, LV_EVENT_CLICKED, NULL);
}

void UIManager::createColorPicker() {
    modalColorPicker = lv_obj_create(lv_display_get_layer_top(lv_display_get_default()));
    lv_obj_set_size(modalColorPicker, 600, 440);
    lv_obj_set_align(modalColorPicker, LV_ALIGN_CENTER);
    lv_obj_add_flag(modalColorPicker, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_pad_all(modalColorPicker, 12, 0);
    lv_obj_set_flex_flow(modalColorPicker, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(modalColorPicker, 6, 0);

    // Header: title + close
    lv_obj_t* header = lv_obj_create(modalColorPicker);
    lv_obj_set_size(header, LV_PCT(100), 56);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_bg_opa(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Select Color");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_align(title, LV_ALIGN_LEFT_MID);

    lv_obj_t* btnClose = lv_btn_create(header);
    lv_obj_set_size(btnClose, 56, 56);
    lv_obj_set_align(btnClose, LV_ALIGN_RIGHT_MID);
    lv_obj_t* lClose = lv_label_create(btnClose);
    lv_label_set_text(lClose, "X");
    lv_obj_set_align(lClose, LV_ALIGN_CENTER);
    lv_obj_set_style_text_font(lClose, &lv_font_montserrat_24, 0);
    lv_obj_add_event_cb(btnClose, [](lv_event_t* e){ ui.closeColorPicker(); }, LV_EVENT_CLICKED, NULL);

    // The product's own colors first: those are what the manufacturer sells and the K2 knows.
    pickerProductLabel = lv_label_create(modalColorPicker);
    lv_obj_set_style_text_font(pickerProductLabel, &lv_font_montserrat_16, 0);
    pickerProductRow = makeSwatchRow(modalColorPicker);

    lv_obj_t* lblOther = lv_label_create(modalColorPicker);
    lv_label_set_text(lblOther, "Other colors");
    lv_obj_set_style_text_font(lblOther, &lv_font_montserrat_16, 0);
    pickerGenericRow = makeSwatchRow(modalColorPicker);
    for (int i = 0; i < FILAMENT_COLOR_COUNT; i++) addSwatch(pickerGenericRow, FILAMENT_COLORS[i]);
}

// Outline every swatch that matches the current color, in both sections.
void UIManager::markPickerSelection() {
    uint32_t cur = currentSpool.getColorHex();
    for (lv_obj_t* row : {pickerProductRow, pickerGenericRow}) {
        uint32_t n = lv_obj_get_child_count(row);
        for (uint32_t i = 0; i < n; i++) {
            lv_obj_t* sw = lv_obj_get_child(row, i);
            bool sel = (uint32_t)(uintptr_t)lv_obj_get_user_data(sw) == cur;
            lv_obj_set_style_border_width(sw, sel ? 4 : 1, 0);
            lv_obj_set_style_border_color(sw, lv_color_hex(sel ? 0x2196F3 : 0x808080), 0);
        }
    }
}

void UIManager::showColorPicker() {
    lv_obj_clean(pickerProductRow);
    FilamentProfile prof;
    bool have = currentSpool.getMaterialId().length() == 5 &&
                filamentDB.getProfileById(currentSpool.getMaterialId().c_str(), prof) && !prof.colors.empty();
    if (have) {
        lv_label_set_text_fmt(pickerProductLabel, "%s colors", prof.name.c_str());
        for (size_t i = 0; i < prof.colors.size(); i++)
            addSwatch(pickerProductRow, prof.colors[i], i < prof.color_names.size() ? prof.color_names[i].c_str() : nullptr);
        lv_obj_clear_flag(pickerProductLabel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(pickerProductRow, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(pickerProductLabel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pickerProductRow, LV_OBJ_FLAG_HIDDEN);
    }
    markPickerSelection();
    lv_obj_scroll_to_y(modalColorPicker, 0, LV_ANIM_OFF);
    lv_obj_clear_flag(modalColorPicker, LV_OBJ_FLAG_HIDDEN);
}

void UIManager::closeColorPicker() {
    lv_obj_add_flag(modalColorPicker, LV_OBJ_FLAG_HIDDEN);
}