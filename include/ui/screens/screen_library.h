#pragma once
#include <lvgl.h>

class ScreenLibrary {
public:
    void init();
    void show();
    void populate();

    lv_obj_t* btnBack; // Made public
    lv_obj_t* grid;     // Made public (assuming this is the new 'list' equivalent)

private:
    lv_obj_t* screen;
    void configureGrid(size_t filamentCount);
};

extern ScreenLibrary screenLibrary;