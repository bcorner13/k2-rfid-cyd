#pragma once
#include <lvgl.h>

class ScreenMain {
public:
    void init();
    void show();
    void update(const class SpoolData& data);
    void setWriteStatus(const char* status, bool success = false, bool neutral = true);
    void updateStatusBar(bool wifiConnected, int8_t rssi, const char* ssid);

    lv_obj_t* screen;
    lv_obj_t* btnSettings;
    lv_obj_t* btnLibrary;
    lv_obj_t* btnWrite;
    lv_obj_t* btnInventory;
    lv_obj_t* labelWriteStatus;
    lv_obj_t* sliderWeight;
    lv_obj_t* labelWeight;
    lv_obj_t* labelName;
    lv_obj_t* ddBrand;
    lv_obj_t* ddType;
    lv_obj_t* colorBlock;       /* colored block, tap opens color picker */
    lv_obj_t* labelHexColor;    /* hex/name label inside colorBlock */

    // Status bar (top 24px row)
    lv_obj_t* statusBar;
    lv_obj_t* labelBattStatus;  /* battery placeholder icon */
    lv_obj_t* labelWifiStatus;  /* wifi icon + SSID/state */
};

extern ScreenMain screenMain;