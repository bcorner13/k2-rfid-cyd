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
#include "screens/screen_inventory.h"
#include "screens/screen_spool_detail.h"
#include "screens/screen_custom_entry.h"
#include "spool_data.h"
#include "system_state.h"

class UIManager {
public:
    UIManager();
    void init();
    void update();

    ScreenMain screenMain;
    ScreenLibrary screenLibrary;
    ScreenSettings screenSettings;
    ScreenAbout screenAbout;
    ScreenInventory screenInventory;
    ScreenSpoolDetail screenSpoolDetail;
    ScreenCustomEntry screenCustomEntry;

    SpoolData currentSpool;

    static void event_handler(lv_event_t* e);

    void showMainScreen();
    void showSettingsScreen();
    void showFilamentLibrary();
    void showAboutScreen();
    void showInventoryScreen();
    void showSpoolDetail(const String& spool_id);
    void showCustomEntry();

    // P1.9: Operation lock & UI guards (FSD Section 12.5)
    void updateButtonStates();  // Enable/disable buttons based on sysState

private:
    void updateDashboardFromSpool(const SpoolData& data);

    // P1.9: Guard check — returns true if system is busy (not IDLE)
    static bool isSystemBusy();

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