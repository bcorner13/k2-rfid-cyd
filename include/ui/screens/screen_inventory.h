#pragma once
/**
 * @file screen_inventory.h
 * @brief Spool list from the local copy of Spoolman (read-only).
 *
 * Every view carries the live/offline status line (offline-inventory spec). Rows hold the spool id
 * in user_data (negative = created on this device, not yet in Spoolman).
 */
#include <lvgl.h>
#include <vector>
#include <local_store.h>

class ScreenInventory {
public:
    void init();
    void show();

    void populate(const std::vector<SpoolRow>& rows, const char* statusText);
    void showMessage(const char* text, const char* statusText);   // empty list with a note

    lv_obj_t* screen = nullptr;
    lv_obj_t* btnBack = nullptr;
    lv_obj_t* list = nullptr;          // scrollable row container
    lv_obj_t* labelCount = nullptr;    // "N spools"
    lv_obj_t* labelStatus = nullptr;   // "Live - synced 14:05"

private:
    void addSpoolRow(const SpoolRow& r);
};

extern ScreenInventory screenInventory;

/// Colour swatch for a spool: plain, or side-by-side stripes for a multi-colour filament.
lv_obj_t* makeSpoolSwatch(lv_obj_t* parent, const SpoolRow& r, int32_t size);
