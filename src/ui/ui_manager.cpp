#include <ui/ui_manager.h>
#include <ui/color_palette.h>
#include <filament_db.h>
#include <config_manager.h>
#include <network_manager.h>
#include <rfid_driver.h>
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

UIManager ui;

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

    screenMain.init();
    screenLibrary.init();
    screenSettings.init();
    screenAbout.init();
    screenInventory.init();
    screenSpoolDetail.init();
    screenCustomEntry.init();

    // Register event handlers once (not on every screen transition)
    lv_obj_add_event_cb(screenMain.btnSettings, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenMain.btnLibrary, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenMain.btnWrite, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenMain.btnReadRfid, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenMain.colorBlock, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenMain.sliderWeight, event_handler, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_add_event_cb(screenSettings.btnBack, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenSettings.btnAbout, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenSettings.swBeep, event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(screenSettings.btnUpdateDB, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenSettings.btnResetWifi, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenSettings.btnRestart, event_handler, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(screenAbout.btnBack, event_handler, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(screenLibrary.btnBack, event_handler, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(screenInventory.btnBack, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenInventory.btnScanTag, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenInventory.btnAddCustom, event_handler, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(screenCustomEntry.btnCancel, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenCustomEntry.btnPrev, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenCustomEntry.btnNext, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenCustomEntry.btnSave, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenCustomEntry.btnSaveWrite, event_handler, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(screenSpoolDetail.btnBack, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenSpoolDetail.btnUpdateWeight, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenSpoolDetail.btnWriteTag, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenSpoolDetail.btnUnlinkTag, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenSpoolDetail.btnArchive, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenSpoolDetail.btnDelete, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenSpoolDetail.btnWeightOk, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screenSpoolDetail.btnWeightCancel, event_handler, LV_EVENT_CLICKED, NULL);

    createColorPicker();
    createOverlay();

    FilamentProfile defaultProfile("0", "Generic", "Generic PLA", "PLA", 0xFFFFFF, "#FFFFFF", 210, 60);
    SpoolData defaultSpool(defaultProfile);
    updateDashboardFromSpool(defaultSpool);

    showMainScreen();
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
    setEnabled(screenMain.btnReadRfid, !busy);
    setEnabled(screenMain.btnWrite, !busy);
    setEnabled(screenMain.btnLibrary, !busy);
    setEnabled(screenMain.colorBlock, !busy);
    setEnabled(screenMain.sliderWeight, !busy);
    // Settings button always active (navigation only)

    // Settings screen: operation buttons disabled when busy
    setEnabled(screenSettings.btnUpdateDB, !busy);
    setEnabled(screenSettings.btnResetWifi, !busy);
    setEnabled(screenSettings.btnRestart, !busy);
    // Back, About, Beep toggle always active

    // Inventory screen: Scan/Add disabled when busy, Back always active
    setEnabled(screenInventory.btnScanTag, !busy);
    setEnabled(screenInventory.btnAddCustom, !busy);

    // Spool detail: action buttons disabled when busy, Back always active
    setEnabled(screenSpoolDetail.btnUpdateWeight, !busy);
    setEnabled(screenSpoolDetail.btnWriteTag, !busy);
    setEnabled(screenSpoolDetail.btnUnlinkTag, !busy);
    setEnabled(screenSpoolDetail.btnArchive, !busy);
    setEnabled(screenSpoolDetail.btnDelete, !busy);
}

// ---------------------------------------------------------------------------
// Event handler with P1.9 busy guard
// ---------------------------------------------------------------------------
void UIManager::event_handler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* obj = (lv_obj_t*) lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED) {
        // Navigation (always allowed regardless of state)
        if (obj == ui.screenMain.btnSettings) { ui.showSettingsScreen(); return; }
        if (obj == ui.screenLibrary.btnBack) { ui.showMainScreen(); return; }
        if (obj == ui.screenSettings.btnBack) { ui.showMainScreen(); return; }
        if (obj == ui.screenSettings.btnAbout) { ui.showAboutScreen(); return; }
        if (obj == ui.screenAbout.btnBack) { ui.showSettingsScreen(); return; }
        if (obj == ui.screenInventory.btnBack) { ui.showMainScreen(); return; }
        if (obj == ui.screenSpoolDetail.btnBack) { ui.showInventoryScreen(); return; }
        if (obj == ui.screenCustomEntry.btnCancel) { ui.showInventoryScreen(); return; }

        // P1.9: Gate all operation-triggering actions when system is busy
        if (isSystemBusy()) {
            Serial.printf("UI: action rejected — system busy (%s)\n", sysState.getStateName());
            return;
        }

        if (obj == ui.screenMain.btnReadRfid) {
            sysState.handleEvent(SystemEvent::READ_REQUEST);
            ui.screenMain.setWriteStatus("Reading...");
            ui.updateButtonStates();

            SpoolData readSpool;
            if (rfid.readCFSTag(readSpool)) {
                ui.updateDashboardFromSpool(readSpool);
                ui.screenMain.setWriteStatus("Read OK", true, false);
                sysState.handleEvent(SystemEvent::OPERATION_SUCCESS);
                feedback.readSuccess();
            } else {
                ui.screenMain.setWriteStatus("No tag / Read failed", false, false);
                sysState.handleEvent(SystemEvent::OPERATION_FAILED);
                feedback.operationFailed();
            }
            ui.updateButtonStates();
        }
        else if (obj == ui.screenMain.btnWrite) {
            sysState.handleEvent(SystemEvent::WRITE_REQUEST);
            ui.screenMain.setWriteStatus("Writing...");
            ui.updateButtonStates();

            if (rfid.writeCFSTag(ui.currentSpool)) {
                ui.screenMain.setWriteStatus("Write OK", true, false);
                sysState.handleEvent(SystemEvent::OPERATION_SUCCESS);
                feedback.writeSuccess();
            } else {
                ui.screenMain.setWriteStatus("Write failed", false, false);
                sysState.handleEvent(SystemEvent::OPERATION_FAILED);
                feedback.operationFailed();
            }
            ui.updateButtonStates();
        }
        else if (obj == ui.screenMain.btnLibrary) {
            ui.showFilamentLibrary();
        }
        else if (obj == ui.screenInventory.btnScanTag) {
            sysState.handleEvent(SystemEvent::SCAN_REQUEST);
            ui.updateButtonStates();

            TagData tag;
            if (rfid.readTag(tag)) {
                feedback.tagDetected();
                String uid = tag.formatUID();
                const SpoolRecord* existing = inventory.getSpoolByUID(uid);
                if (existing) {
                    // Known spool — reconcile weight
                    auto reconcile = RFIDDriver::reconcileWeight(
                        tag.remaining_weight_g, existing->current_weight_g);
                    if (!reconcile.in_sync) {
                        Serial.printf("Weight mismatch: tag=%ug inv=%ug\n",
                                      reconcile.tag_weight_g, reconcile.inventory_weight_g);
                        // Update inventory with tag weight (tag is source of truth)
                        inventory.updateWeight(existing->spool_id, reconcile.tag_weight_g);
                    }
                    sysState.handleEvent(SystemEvent::OPERATION_SUCCESS);
                    ui.updateButtonStates();
                    ui.showSpoolDetail(existing->spool_id);
                    return;
                }
                sysState.handleEvent(SystemEvent::OPERATION_SUCCESS);
            } else {
                sysState.handleEvent(SystemEvent::OPERATION_FAILED);
                feedback.operationFailed();
            }
            ui.updateButtonStates();
            ui.screenInventory.populate();
        }
        else if (obj == ui.screenInventory.btnAddCustom) {
            ui.showCustomEntry();
        }
        else if (obj == ui.screenSpoolDetail.btnWeightCancel) {
            // Close modal (no busy guard needed)
            lv_obj_add_flag(ui.screenSpoolDetail.modalWeight, LV_OBJ_FLAG_HIDDEN);
        }
        else if (obj == ui.screenSpoolDetail.btnUpdateWeight) {
            // Show weight input modal
            lv_obj_clear_flag(ui.screenSpoolDetail.modalWeight, LV_OBJ_FLAG_HIDDEN);
        }
        else if (obj == ui.screenSpoolDetail.btnWeightOk) {
            // Save weight from spinbox
            int32_t newWeight = lv_spinbox_get_value(ui.screenSpoolDetail.spinboxWeight);
            inventory.updateWeight(ui.screenSpoolDetail.currentSpoolId, (uint32_t)newWeight);
            lv_obj_add_flag(ui.screenSpoolDetail.modalWeight, LV_OBJ_FLAG_HIDDEN);
            // Refresh the detail screen
            ui.screenSpoolDetail.loadSpool(ui.screenSpoolDetail.currentSpoolId);
            feedback.spoolSaved();
            Serial.printf("Weight updated: %s → %dg\n",
                          ui.screenSpoolDetail.currentSpoolId.c_str(), (int)newWeight);
        }
        else if (obj == ui.screenSpoolDetail.btnWriteTag) {
            const SpoolRecord* rec = inventory.getSpoolById(ui.screenSpoolDetail.currentSpoolId);
            if (!rec) return;

            sysState.handleEvent(SystemEvent::WRITE_REQUEST);
            ui.updateButtonStates();

            // Build SpoolData from SpoolRecord for CFS tag write
            SpoolData writeData;
            writeData.setType(rec->material_type.c_str());
            writeData.setColor(strtoul(rec->color_hex.c_str(), nullptr, 16));
            writeData.setWeight(rec->current_weight_g);

            if (rfid.writeCFSTag(writeData)) {
                sysState.handleEvent(SystemEvent::OPERATION_SUCCESS);
                feedback.writeSuccess();
                Serial.printf("Tag written for spool %s\n", rec->spool_id.c_str());
            } else {
                sysState.handleEvent(SystemEvent::OPERATION_FAILED);
                feedback.operationFailed();
                Serial.println("Tag write failed");
            }
            ui.updateButtonStates();
        }
        else if (obj == ui.screenSpoolDetail.btnUnlinkTag) {
            inventory.unlinkTag(ui.screenSpoolDetail.currentSpoolId);
            ui.screenSpoolDetail.loadSpool(ui.screenSpoolDetail.currentSpoolId);
            Serial.printf("Tag unlinked from spool %s\n",
                          ui.screenSpoolDetail.currentSpoolId.c_str());
        }
        else if (obj == ui.screenSpoolDetail.btnArchive) {
            String id = ui.screenSpoolDetail.currentSpoolId;
            inventory.archiveSpool(id);
            Serial.printf("Spool archived: %s\n", id.c_str());
            ui.showInventoryScreen();
        }
        else if (obj == ui.screenSpoolDetail.btnDelete) {
            String id = ui.screenSpoolDetail.currentSpoolId;
            inventory.deleteSpool(id);
            Serial.printf("Spool deleted: %s\n", id.c_str());
            ui.showInventoryScreen();
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

            if (obj == ui.screenCustomEntry.btnSaveWrite) {
                // Navigate to spool detail for tag writing
                ui.showSpoolDetail(newId);
            } else {
                ui.showInventoryScreen();
            }
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
            } else {
                sysState.handleEvent(SystemEvent::DB_UPDATE_FAILED);
                feedback.dbUpdateFailed();
            }
            ui.updateButtonStates();
        }
        else if (obj == ui.screenSettings.btnResetWifi) {
            network.startConfigPortal();
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
                size_t idx = (size_t)lv_obj_get_user_data(invRow);
                auto active = inventory.getAllActive();
                if (idx < active.size()) {
                    ui.showSpoolDetail(active[idx]->spool_id);
                }
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
        if (obj == ui.screenMain.sliderWeight) {
            if (isSystemBusy()) return;  // P1.9: guard slider too
            int weight = lv_slider_get_value(obj);
            ui.currentSpool.setWeight(weight);
            lv_label_set_text_fmt(ui.screenMain.labelWeight, "%dg", weight);
        }
        else if (obj == ui.screenSettings.swBeep) {
            config.data.beep_enabled = lv_obj_has_state(obj, LV_STATE_CHECKED);
            config.save();
        }
    }
}

void UIManager::showMainScreen() {
    updateDashboardFromSpool(currentSpool);
    screenMain.setWriteStatus("Ready");
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

void UIManager::showSpoolDetail(const String& spool_id) {
    screenSpoolDetail.loadSpool(spool_id);
    screenSpoolDetail.show();
    updateButtonStates();
}

void UIManager::showCustomEntry() {
    screenCustomEntry.reset();
    screenCustomEntry.show();
}

void UIManager::showInventoryScreen() {
    screenInventory.populate();
    screenInventory.show();
    updateButtonStates();

    // Register tap handlers for inventory list rows (dynamic, recreated by populate)
    uint32_t count = lv_obj_get_child_cnt(screenInventory.list);
    for (uint32_t i = 0; i < count; i++) {
        lv_obj_t* row = lv_obj_get_child(screenInventory.list, i);
        lv_obj_add_event_cb(row, event_handler, LV_EVENT_CLICKED, NULL);
    }
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

void UIManager::updateDashboardFromSpool(const SpoolData& data) {
    currentSpool = data;
    screenMain.update(data);
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

void UIManager::createColorPicker() {
    modalColorPicker = lv_obj_create(lv_display_get_layer_top(lv_display_get_default()));
    lv_obj_set_size(modalColorPicker, 360, 340);
    lv_obj_set_align(modalColorPicker, LV_ALIGN_CENTER);
    lv_obj_add_flag(modalColorPicker, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_pad_all(modalColorPicker, 12, 0);

    lv_obj_t* title = lv_label_create(modalColorPicker);
    lv_label_set_text(title, "Select Color");
    lv_obj_set_align(title, LV_ALIGN_TOP_MID);
    lv_obj_set_y(title, 4);

    lv_obj_t* btnClose = lv_btn_create(modalColorPicker);
    lv_obj_set_size(btnClose, 56, 56);
    lv_obj_set_align(btnClose, LV_ALIGN_TOP_RIGHT);
    lv_obj_set_x(btnClose, -4);
    lv_obj_set_y(btnClose, 0);
    lv_obj_t* lClose = lv_label_create(btnClose);
    lv_label_set_text(lClose, "X");
    lv_obj_set_align(lClose, LV_ALIGN_CENTER);
    lv_obj_set_style_text_font(lClose, &lv_font_montserrat_24, 0);
    lv_obj_add_event_cb(btnClose, [](lv_event_t* e){ ui.closeColorPicker(); }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* grid = lv_obj_create(modalColorPicker);
    lv_obj_set_size(grid, 320, 260);
    lv_obj_set_align(grid, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_y(grid, -8);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_style_pad_row(grid, 6, 0);
    lv_obj_set_style_pad_column(grid, 6, 0);

    static lv_coord_t col_dsc[] = {56, 56, 56, 56, 56, LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {56, 56, 56, 56, 56, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);

    for(int i=0; i<FILAMENT_COLOR_COUNT; i++) {
        lv_obj_t* btn = lv_btn_create(grid);
        lv_obj_set_size(btn, 56, 56);
        lv_obj_set_style_bg_color(btn, lv_color_hex(FILAMENT_COLORS[i]), 0);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_CENTER, i%5, 1, LV_GRID_ALIGN_CENTER, i/5, 1);

        lv_obj_add_event_cb(btn, [](lv_event_t* e){
            lv_obj_t* t = (lv_obj_t*)lv_event_get_target(e);
            lv_color_t c = lv_obj_get_style_bg_color(t, LV_PART_MAIN);
            lv_color32_t c32 = lv_color_to_32(c, LV_OPA_COVER);
            uint32_t hex =
                ((uint32_t)c32.red   << 16) |
                ((uint32_t)c32.green << 8)  |
                ((uint32_t)c32.blue);
            ui.currentSpool.setColor(hex);
            ui.updateDashboardFromSpool(ui.currentSpool);
            ui.closeColorPicker();
        }, LV_EVENT_CLICKED, NULL);
    }
}

void UIManager::showColorPicker() {
    lv_obj_clear_flag(modalColorPicker, LV_OBJ_FLAG_HIDDEN);
}

void UIManager::closeColorPicker() {
    lv_obj_add_flag(modalColorPicker, LV_OBJ_FLAG_HIDDEN);
}