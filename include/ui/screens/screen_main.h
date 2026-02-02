#pragma once
#include <lvgl.h>
#include "../widgets/spool_widget.h"

class ScreenMain {
public:
    void init();
    void show();
    void update(const class SpoolData& data);

    lv_obj_t* screen;
    lv_obj_t* btnSettings;
    lv_obj_t* btnLibrary;
    lv_obj_t* btnWrite;
    lv_obj_t* sliderWeight;
    lv_obj_t* labelWeight;
    lv_obj_t* ddBrand;
    lv_obj_t* ddType;
    lv_obj_t* labelHexColor;
    SpoolWidget spoolWidget;
};

extern ScreenMain screenMain;