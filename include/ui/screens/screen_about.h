#pragma once
#include <lvgl.h>

class ScreenAbout {
public:
    void init();
    void show();

    lv_obj_t* screen;
    lv_obj_t* btnBack;

private:
    lv_obj_t* labelTitle;
    lv_obj_t* labelBoard;
    lv_obj_t* labelDisplay;
    lv_obj_t* labelRFID;
    lv_obj_t* labelFWInfo;
    lv_obj_t* labelChipInfo;
    lv_obj_t* labelMemInfo;
    lv_obj_t* labelFlashInfo;
    lv_obj_t* labelStorageInfo;
    lv_obj_t* labelWifiStatus;
    lv_obj_t* labelRFIDLive;
};

extern ScreenAbout screenAbout;