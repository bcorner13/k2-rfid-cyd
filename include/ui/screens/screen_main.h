#pragma once
#include <lvgl.h>

class ScreenMain {
public:
    void init();
    void show();
    void update(const class SpoolData& data);
    void setWriteStatus(const char* status);

    lv_obj_t* screen;
    lv_obj_t* btnSettings;
    lv_obj_t* btnLibrary;
    lv_obj_t* btnWrite;
    lv_obj_t* btnReadRfid;
    lv_obj_t* labelWriteStatus;
    lv_obj_t* sliderWeight;
    lv_obj_t* labelWeight;
    lv_obj_t* ddBrand;
    lv_obj_t* ddType;
    lv_obj_t* colorBlock;     /* colored block, tap opens color picker */
    lv_obj_t* labelHexColor;  /* hex/name label inside colorBlock */
};

extern ScreenMain screenMain;