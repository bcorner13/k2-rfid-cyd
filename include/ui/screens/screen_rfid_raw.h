#pragma once
#include <lvgl.h>

/**
 * @file screen_rfid_raw.h
 * @brief RFID Raw Data screen — shows the decoded CFS payload in a structured table.
 *
 * Layout (800×480):
 *   - Title "Creality RFID Tag Data" (top-centre)
 *   - Back button (top-left)
 *   - Raw payload string label (below title)
 *   - 8-column lv_table with header row (field names) + data row (parsed values)
 *
 * Field columns: date(M DD YY) | venderId | batch | filamentId | color | filamentLen | serialNum | reserve
 */
class ScreenRfidRaw {
public:
    void init();
    void show();

    lv_obj_t* screen;
    lv_obj_t* btnBack;

private:
    lv_obj_t* labelPayload;   ///< Raw 40-char payload string label
    lv_obj_t* table;          ///< 8-column breakdown table

    /** LVGL draw-task callback — styles header row (row 0) with dark-blue bg + cyan text. */
    static void table_draw_cb(lv_event_t* e);

    /** Populate table row 1 with fields parsed from a 40-char CFS payload. */
    void populateTable(const char* payload);
};

extern ScreenRfidRaw screenRfidRaw;
