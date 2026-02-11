#include <ui/ui_manager.h>
#include <ui/color_palette.h>
#include <filament_db.h>
#include <config_manager.h>
#include <network_manager.h>
#include <rfid_driver.h>

// Include screen headers from the new include path
#include <ui/screens/screen_main.h>
#include <ui/screens/screen_library.h>
#include <ui/screens/screen_settings.h>
#include <ui/screens/screen_about.h>

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

void UIManager::event_handler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* obj = (lv_obj_t*) lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED) {
        if (obj == ui.screenMain.btnSettings) ui.showSettingsScreen();
        else if (obj == ui.screenMain.btnLibrary) ui.showFilamentLibrary();
        else if (obj == ui.screenLibrary.btnBack) ui.showMainScreen();
        else if (obj == ui.screenSettings.btnBack) ui.showMainScreen();
        else if (obj == ui.screenSettings.btnAbout) ui.showAboutScreen();
        else if (obj == ui.screenAbout.btnBack) ui.showSettingsScreen();

        else if (obj == ui.screenMain.btnReadRfid) {
            SpoolData readSpool;
            if (rfid.readCFSTag(readSpool)) {
                ui.updateDashboardFromSpool(readSpool);
                ui.screenMain.setWriteStatus("Read OK", true, false);
            } else {
                ui.screenMain.setWriteStatus("No tag / Read failed", false, false);
            }
        }
        else if (obj == ui.screenMain.btnWrite) {
            if (rfid.writeCFSTag(ui.currentSpool)) {
                ui.screenMain.setWriteStatus("Write OK", true, false);
            } else {
                ui.screenMain.setWriteStatus("Write failed", false, false);
            }
        }
        else if (obj == ui.screenMain.colorBlock) {
            ui.showColorPicker();  // tap color block to pick color
        }
        else if (obj == ui.screenSettings.btnUpdateDB) {
            network.updateFilamentDB();
        }
        else if (obj == ui.screenSettings.btnResetWifi) {
            network.startConfigPortal();
        }
        else if (obj == ui.screenSettings.btnRestart) {
            ESP.restart();
        }

        else {
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
}

void UIManager::showSettingsScreen() {
    screenSettings.show();
}

void UIManager::showAboutScreen() {
    screenAbout.show();
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