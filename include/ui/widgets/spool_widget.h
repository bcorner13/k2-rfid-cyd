#pragma once
#include <lvgl.h>

class SpoolWidget {
public:
    void create(lv_obj_t* parent);
    void update(const char* type, uint32_t color, uint32_t weight);
    lv_obj_t* getContainer() { return container; }

private:
    lv_obj_t* container;
    lv_obj_t* filament;
    lv_obj_t* labelType;
    lv_obj_t* labelWeight; // Added this line
};