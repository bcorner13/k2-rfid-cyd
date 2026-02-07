#include "ui/screens/screen_filament_select.h"
#include "filament_db.h"
#include "system_state.h"
#include <lvgl.h>

/* ----------------------------------------------------
   Static screen objects
---------------------------------------------------- */
// Removed static global declarations for screen and cont

static bool screen_built = false;

/* ----------------------------------------------------
   Layout constants
---------------------------------------------------- */
static const uint8_t  FILAMENT_COLUMNS = 2;
static const uint16_t BUTTON_HEIGHT    = 60;
static const uint16_t TITLE_HEIGHT     = 50;

/* ----------------------------------------------------
   Init screen (idempotent, safe to call many times)
---------------------------------------------------- */
void ScreenFilamentSelect::init() {
    if (screen_built) return;
    screen_built = true;

    Serial.println("BUILD FILAMENT SCREEN");

    screen = lv_obj_create(NULL);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    /* Title */
    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "Select Filament");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_align(title, LV_ALIGN_TOP_MID); // Changed from lv_obj_align
    lv_obj_set_y(title, 10); // Set y offset

    /* Scroll container */
    cont = lv_obj_create(screen);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_align(cont, LV_ALIGN_BOTTOM_MID); // Changed from lv_obj_align
    lv_obj_set_y(cont, 0); // Set y offset

    lv_obj_set_style_pad_top(cont, 60, 0);
    lv_obj_set_style_pad_all(cont, 10, 0);
    lv_obj_set_style_pad_row(cont, 10, 0);
    lv_obj_set_style_pad_column(cont, 10, 0);

    lv_obj_set_scroll_dir(cont, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_AUTO);

    lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(
        cont,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START
    );
}


/* ----------------------------------------------------
   Show screen
---------------------------------------------------- */
void ScreenFilamentSelect::show() {
    lv_screen_load(screen);
    lv_timer_handler();

    lv_obj_clean(cont);

    lv_coord_t w = lv_obj_get_width(cont);
    if (w <= 0) w = 400;
    lv_coord_t btn_w = (w - 20) / 2;

    /* Button count follows material_database.json (filamentDB.getCache()). */
    const auto& filaments = filamentDB.getCache();
    size_t idx = 0;

    for (const auto& profile : filaments) {
        lv_obj_t* btn = lv_btn_create(cont);
        lv_obj_set_user_data(btn, (void*)idx);
        idx++;

        lv_obj_set_size(btn, btn_w, 72);
        lv_obj_set_style_radius(btn, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(btn, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(btn, lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t* swatch = lv_obj_create(btn);
        lv_obj_set_size(swatch, 19, 19);
        lv_obj_set_align(swatch, LV_ALIGN_LEFT_MID);
        lv_obj_set_x(swatch, 3);
        lv_obj_set_style_bg_color(swatch, lv_color_hex(profile.color_hex), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(swatch, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(swatch, 4, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t* label = lv_label_create(btn);
        lv_label_set_text(label, profile.name.c_str());
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        lv_obj_set_width(label, LV_PCT(100));
        lv_obj_set_align(label, LV_ALIGN_RIGHT_MID);
        lv_obj_set_x(label, -6);
        lv_obj_set_style_text_color(label, lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

ScreenFilamentSelect screenFilamentSelect;