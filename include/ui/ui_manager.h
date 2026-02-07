#pragma once
/**
 * @file ui_manager.h
 * @brief LVGL 9 UI controller: screens, events, current spool, color picker.
 *
 * Owns ScreenMain, ScreenLibrary, ScreenSettings, ScreenAbout and routes
 * events (library selection, write tag, weight slider, color picker, etc.).
 * currentSpool is the in-memory spool; updateDashboardFromSpool() pushes
 * it to the main screen.
 */

#include <lvgl.h>
#include "screens/screen_main.h"
#include "screens/screen_library.h"
#include "screens/screen_settings.h"
#include "screens/screen_about.h"
#include "spool_data.h"

class UIManager {
public:
    UIManager();
    void init();
    void update();

    ScreenMain screenMain;
    ScreenLibrary screenLibrary;
    ScreenSettings screenSettings;
    ScreenAbout screenAbout;

    SpoolData currentSpool;

    static void event_handler(lv_event_t* e);

    void showMainScreen();
    void showSettingsScreen();
    void showFilamentLibrary();
    void showAboutScreen(); // This was missing

private:
    void updateDashboardFromSpool(const SpoolData& data);

    // Top Layer
    lv_obj_t* layerTop{nullptr};
    lv_obj_t* labelBattery{nullptr};
    void createOverlay();
    void updateBattery(float voltage);

    // Color Picker
    lv_obj_t* modalColorPicker{nullptr};
    void createColorPicker();
    void showColorPicker();
    void closeColorPicker();
};

extern UIManager ui;