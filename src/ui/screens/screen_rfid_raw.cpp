#include "ui/screens/screen_rfid_raw.h"
#include "rfid_driver.h"
#include <cstring>
#include <cstdio>

ScreenRfidRaw screenRfidRaw;

// ---------------------------------------------------------------------------
// Column definitions — header label and pixel width (total must fit 800 px).
// Header row uses the field name / format hint so the user can read the data.
// ---------------------------------------------------------------------------
static constexpr int kColCount = 8;
static const struct { const char* hdr; uint16_t w; } kCols[kColCount] = {
    { "Date",        100 },   // date  (pos 0-4)
    { "venderId",     90 },   // vendor (pos 5-8)
    { "batch",        70 },   // batch  (pos 9-10)
    { "filamentId",  110 },   // sep+type (pos 11-16)
    { "color",       100 },   // pfx+hex  (pos 17-23)
    { "filamentLen", 110 },   // length   (pos 24-27)
    { "serialNum",   110 },   // serial   (pos 28-33)
    { "reserve",     110 },   // reserve  (pos 34-39)
    // ───────── total ────
    //  100+90+70+110+100+110+110+110 = 800 px
};

// ---------------------------------------------------------------------------
// Draw-task callback: style header row (row 0) with dark-blue bg + cyan text.
// ---------------------------------------------------------------------------
void ScreenRfidRaw::table_draw_cb(lv_event_t* e) {
    lv_draw_task_t*    dt   = lv_event_get_draw_task(e);
    if (!dt) return;

    lv_draw_dsc_base_t* base = (lv_draw_dsc_base_t*)lv_draw_task_get_draw_dsc(dt);
    if (!base) return;
    if (base->part != LV_PART_ITEMS) return;  // only table cells
    if (base->id1  != 0)             return;  // only header row

    lv_draw_task_type_t type = lv_draw_task_get_type(dt);
    if (type == LV_DRAW_TASK_TYPE_FILL) {
        lv_draw_fill_dsc_t* fill = (lv_draw_fill_dsc_t*)lv_draw_task_get_draw_dsc(dt);
        fill->color = lv_color_hex(0x0A2040);
        fill->opa   = LV_OPA_COVER;
    } else if (type == LV_DRAW_TASK_TYPE_LABEL) {
        lv_draw_label_dsc_t* lbl = (lv_draw_label_dsc_t*)lv_draw_task_get_draw_dsc(dt);
        lbl->color = lv_color_hex(0x00C8FF);
    }
}

// ---------------------------------------------------------------------------
// init() — build the screen widgets once at startup.
// ---------------------------------------------------------------------------
void ScreenRfidRaw::init() {
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    // --- Back button (top-left) ---
    btnBack = lv_btn_create(screen);
    lv_obj_set_size(btnBack, 60, 44);
    lv_obj_align(btnBack, LV_ALIGN_TOP_LEFT, 12, 10);
    lv_obj_t* lBack = lv_label_create(btnBack);
    lv_label_set_text(lBack, LV_SYMBOL_LEFT);
    lv_obj_center(lBack);

    // --- Title ---
    lv_obj_t* labelTitle = lv_label_create(screen);
    lv_label_set_text(labelTitle, "Creality RFID Tag Data");
    lv_obj_set_style_text_font(labelTitle,  &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(labelTitle, lv_color_white(), 0);
    lv_obj_align(labelTitle, LV_ALIGN_TOP_MID, 0, 18);

    // --- Raw payload label (grey, monospace-feel) ---
    labelPayload = lv_label_create(screen);
    lv_obj_set_style_text_font(labelPayload,  &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(labelPayload, lv_color_hex(0x888888), 0);
    lv_label_set_long_mode(labelPayload, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(labelPayload, 760);
    lv_label_set_text(labelPayload, "---");
    lv_obj_align(labelPayload, LV_ALIGN_TOP_MID, 0, 52);

    // --- Table ---
    table = lv_table_create(screen);
    lv_table_set_column_count(table, (uint32_t)kColCount);

    // Column widths
    for (int c = 0; c < kColCount; c++) {
        lv_table_set_column_width(table, (uint32_t)c, kCols[c].w);
    }

    // Header row (row 0) — field names
    for (int c = 0; c < kColCount; c++) {
        lv_table_set_cell_value(table, 0, (uint32_t)c, kCols[c].hdr);
    }

    // Placeholder data row (row 1)
    for (int c = 0; c < kColCount; c++) {
        lv_table_set_cell_value(table, 1, (uint32_t)c, "---");
    }

    // Table body styles
    lv_obj_set_style_bg_color(table,   lv_color_hex(0x0D1120), 0);
    lv_obj_set_style_bg_opa(table,     LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(table, 0, 0);

    // Cell styles (all rows)
    lv_obj_set_style_text_font(table,   &lv_font_montserrat_14, LV_PART_ITEMS);
    lv_obj_set_style_text_color(table,  lv_color_hex(0xDDDDDD), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(table,    lv_color_hex(0x111827), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(table,      LV_OPA_COVER,           LV_PART_ITEMS);
    lv_obj_set_style_border_color(table, lv_color_hex(0x2A3A55), LV_PART_ITEMS);
    lv_obj_set_style_border_width(table, 1,                      LV_PART_ITEMS);
    lv_obj_set_style_pad_ver(table,     8,  LV_PART_ITEMS);
    lv_obj_set_style_pad_hor(table,     10, LV_PART_ITEMS);

    // Place table below the payload label
    lv_obj_align(table, LV_ALIGN_TOP_MID, 0, 82);

    // Register draw callback to style header row
    lv_obj_add_flag(table, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    lv_obj_add_event_cb(table, table_draw_cb, LV_EVENT_DRAW_TASK_ADDED, nullptr);
}

// ---------------------------------------------------------------------------
// populateTable() — parse a CFS payload string and fill table row 1.
// ---------------------------------------------------------------------------
void ScreenRfidRaw::populateTable(const char* payload) {
    const size_t len = payload ? strlen(payload) : 0;
    if (len < 34) {
        for (int c = 0; c < kColCount; c++) {
            lv_table_set_cell_value(table, 1, (uint32_t)c, "---");
        }
        return;
    }

    char dateFmt[16], vendor[8], batch[8], matId[8], color[10], length[8], serial[8], reserve[8];

    // date (pos 0-4): prefix(1) + month_hex(1) + day(1) + year_2(2)
    // Month is a hex digit: '1'-'9' = Jan-Sep, 'A'=Oct, 'B'=Nov, 'C'=Dec.
    {
        static const char* kMon[13] = {
            "?","Jan","Feb","Mar","Apr","May","Jun",
            "Jul","Aug","Sep","Oct","Nov","Dec"
        };
        char mch = payload[1];
        int mi = (mch >= '1' && mch <= '9') ? (mch - '0') :
                 (mch >= 'A' && mch <= 'C') ? (mch - 'A' + 10) : 0;
        snprintf(dateFmt, sizeof(dateFmt), "%s %c '%.2s",
                 kMon[mi], payload[2], payload + 3);
    }

    // vendor (pos 5-8): 4 chars as-is
    snprintf(vendor, sizeof(vendor), "%.4s", payload + 5);

    // batch (pos 9-10): 2 chars as-is
    snprintf(batch, sizeof(batch), "%.2s", payload + 9);

    // filamentId (pos 11-16): separator(1) + material type(5) = 6 chars
    snprintf(matId, sizeof(matId), "%.6s", payload + 11);

    // color (pos 18-23): skip Creality's '0' prefix, show as xRRGGBB
    snprintf(color, sizeof(color), "x%.6s", payload + 18);

    // length (pos 24-27): 4 chars as-is (mm × 10 or raw value)
    snprintf(length, sizeof(length), "%.4s", payload + 24);

    // serial (pos 28-33): 6 chars as-is
    snprintf(serial, sizeof(serial), "%.6s", payload + 28);

    // reserve (pos 34-39): 6 chars if available, else dashes
    if (len >= 40) {
        snprintf(reserve, sizeof(reserve), "%.6s", payload + 34);
    } else {
        strncpy(reserve, "------", sizeof(reserve) - 1);
        reserve[sizeof(reserve) - 1] = '\0';
    }

    lv_table_set_cell_value(table, 1, 0, dateFmt);
    lv_table_set_cell_value(table, 1, 1, vendor);
    lv_table_set_cell_value(table, 1, 2, batch);
    lv_table_set_cell_value(table, 1, 3, matId);
    lv_table_set_cell_value(table, 1, 4, color);
    lv_table_set_cell_value(table, 1, 5, length);
    lv_table_set_cell_value(table, 1, 6, serial);
    lv_table_set_cell_value(table, 1, 7, reserve);
}

// ---------------------------------------------------------------------------
// show() — called each time the user navigates to this screen.
// ---------------------------------------------------------------------------
void ScreenRfidRaw::show() {
    const char* payload = rfid.getLastPayload();

    if (payload && payload[0]) {
        // Show the raw payload string above the table
        char buf[56];
        snprintf(buf, sizeof(buf), "Payload: %s", payload);
        lv_label_set_text(labelPayload, buf);
        populateTable(payload);
    } else {
        lv_label_set_text(labelPayload, "No tag read yet — hold a spool over the reader.");
        for (int c = 0; c < kColCount; c++) {
            lv_table_set_cell_value(table, 1, (uint32_t)c, "---");
        }
    }

    lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
}
