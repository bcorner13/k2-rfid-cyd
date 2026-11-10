#pragma once
/**
 * @file screen_spool_detail.h
 * @brief One spool from the local copy, read-only, with "Write tag for this spool".
 *
 * Editing (weight, location, archive, delete) stays in Spoolman's web UI (offline-inventory spec,
 * non-goals).
 */
#include <lvgl.h>
#include <local_store.h>

class ScreenSpoolDetail {
public:
    void init();
    void show();
    void loadSpool(const SpoolRow& r, const char* statusText);

    lv_obj_t* screen = nullptr;
    lv_obj_t* btnBack = nullptr;
    lv_obj_t* btnWriteTag = nullptr;
    int currentSpoolId = 0;

private:
    void lazyInit();
    lv_obj_t* header = nullptr;
    lv_obj_t* swatch = nullptr;
    lv_obj_t* labelName = nullptr;
    lv_obj_t* labelInfo = nullptr;
    lv_obj_t* labelStatus = nullptr;
};

extern ScreenSpoolDetail screenSpoolDetail;
