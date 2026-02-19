#pragma once
#include <lvgl.h>

class ScreenInventory {
public:
    void init();
    void show();
    void populate();  // Rebuild list from inventory data

    lv_obj_t* screen;
    lv_obj_t* btnBack;
    lv_obj_t* btnScanTag;
    lv_obj_t* btnAddCustom;
    lv_obj_t* list;           // Scrollable spool list container
    lv_obj_t* labelCount;     // "N spools" counter in title bar

private:
    void addSpoolRow(size_t index, const char* brand, const char* name,
                     const char* material, uint32_t color_hex,
                     uint32_t current_weight_g, uint32_t initial_weight_g,
                     uint8_t status);
};

extern ScreenInventory screenInventory;
