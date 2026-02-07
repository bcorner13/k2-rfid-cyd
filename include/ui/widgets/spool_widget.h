#pragma once
#include <lvgl.h>

class SpoolWidget {
public:
    void create(lv_obj_t* parent);
    void update(const char* type, uint32_t color, uint32_t weight);
    lv_obj_t* getContainer() { return container; }
    lv_obj_t* getFilamentArc() { return filament; }
    lv_obj_t* getCore() { return core; }

private:
    lv_obj_t* container;
    lv_obj_t* filament;
    lv_obj_t* core;       // center hub (tap opens color picker)
    lv_obj_t* labelType;
    lv_obj_t* labelWeight;
};