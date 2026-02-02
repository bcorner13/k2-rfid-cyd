#pragma once
#include <lvgl.h>

class ScreenSettings {
public:
    void init();
    void show();

    lv_obj_t* screen;
    lv_obj_t* btnBack;
    lv_obj_t* swBeep;
    lv_obj_t* btnUpdateDB;
    lv_obj_t* btnResetWifi;
    lv_obj_t* btnRestart;
    lv_obj_t* btnAbout;
};

extern ScreenSettings screenSettings;