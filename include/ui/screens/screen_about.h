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
    lv_obj_t* labelESPVersion;
    lv_obj_t* labelLVGLVersion;
    lv_obj_t* labelChipInfo;
    lv_obj_t* labelMemInfo;
    lv_obj_t* labelFlashInfo;
    lv_obj_t* labelStorageInfo;
};

extern ScreenAbout screenAbout;