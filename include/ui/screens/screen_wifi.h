#pragma once
#include <lvgl.h>

class ScreenWifi {
public:
    void init();
    void show();
    void setStatus(const char* text);

    lv_obj_t* screen;
    lv_obj_t* labelStatus;
    lv_obj_t* btnBack;
};

extern ScreenWifi screenWifi;
