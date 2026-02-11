#include <ui/screens/screen_library.h> // Updated include path
#include <filament_db.h> // Updated include path
#include <Arduino.h>

ScreenLibrary screenLibrary;

/* Grid size is driven by filament count from material_database.json (see populate()). */
static constexpr uint8_t GRID_COLS = 4;
static constexpr uint16_t GRID_MAX_ROWS = 50;   /* cap: 4*50 = 200 filaments max */
static lv_coord_t col_dsc[GRID_COLS + 1];
static lv_coord_t row_dsc[GRID_MAX_ROWS + 1];

/* Button height and visible area; more rows visible with smaller height */
static const uint16_t BUTTON_HEIGHT = 100;
static const uint16_t GRID_VISIBLE_HEIGHT = 420;   /* ~4 rows visible */
static const uint16_t GRID_MARGIN_H_PX = 10;       /* ~1–2 mm each side at typical DPI */

void ScreenLibrary::init()
{
    screen = lv_obj_create(nullptr);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);

    // Title
    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "Select Filament");
    lv_obj_set_align(title, LV_ALIGN_TOP_MID);
    lv_obj_set_y(title, 10);
    lv_obj_set_style_text_color(title, lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Back button
    btnBack = lv_btn_create(screen);
    lv_obj_set_size(btnBack, 80, 36);
    lv_obj_set_align(btnBack, LV_ALIGN_TOP_LEFT); // Changed from lv_obj_align
    lv_obj_set_x(btnBack, 10); // Set x offset
    lv_obj_set_y(btnBack, 8); // Set y offset

    lv_obj_t* lblBack = lv_label_create(btnBack);
    lv_label_set_text(lblBack, "< Back");
    lv_obj_set_align(lblBack, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(lblBack, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);

    // Grid container: full width minus small left/right margin (~1–2 mm)
    grid = lv_obj_create(screen);
    lv_coord_t disp_w = lv_disp_get_hor_res(lv_display_get_default());
    lv_obj_set_size(grid, disp_w - (2 * GRID_MARGIN_H_PX), GRID_VISIBLE_HEIGHT);
    lv_obj_set_align(grid, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_y(grid, -10);
    lv_obj_set_scroll_dir(grid, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_ON);
    lv_obj_set_style_width(grid, 14, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(grid, lv_color_hex(0x808080), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(grid, LV_OPA_COVER, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(grid, 7, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);

    // Populate and configure grid
    populate();
}


void ScreenLibrary::configureGrid(size_t filamentCount)
{
    /* Rows derived from current filament count (material_database.json); capped by GRID_MAX_ROWS. */
    uint16_t rows = (filamentCount + GRID_COLS - 1) / GRID_COLS;
    if (rows == 0) rows = 1;
    if (rows > GRID_MAX_ROWS) rows = GRID_MAX_ROWS;

    for (uint8_t i = 0; i < GRID_COLS; i++) {
        col_dsc[i] = LV_GRID_FR(1);
    }
    col_dsc[GRID_COLS] = LV_GRID_TEMPLATE_LAST;

    /* Fixed row height so buttons are never squashed */
    for (uint16_t i = 0; i < rows; i++) {
        row_dsc[i] = (lv_coord_t)BUTTON_HEIGHT;
    }
    row_dsc[rows] = LV_GRID_TEMPLATE_LAST;

    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
}


void ScreenLibrary::populate()
{
    lv_obj_clean(grid);

    /* Button count = number of filaments in material_database.json (may change). */
    const auto& filaments = filamentDB.getCache();
    size_t count = filaments.size();

    configureGrid(count);

    for (size_t i = 0; i < count; i++) {
        uint8_t col = i % GRID_COLS;
        uint8_t row = i / GRID_COLS;

        if (row >= GRID_MAX_ROWS) break;

        lv_obj_t* cell = lv_btn_create(grid);
        lv_obj_set_grid_cell(
            cell,
            LV_GRID_ALIGN_STRETCH, col, 1,
            LV_GRID_ALIGN_STRETCH, row, 1
        );
        lv_obj_set_height(cell, BUTTON_HEIGHT);
        lv_obj_set_style_pad_all(cell, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(cell, lv_color_hex(0x2196F3), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_user_data(cell, (void*)i);
        lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

        /* Inner flex row: swatch left (fixed), label right (takes rest) so text never overlaps swatch */
        lv_obj_t* inner = lv_obj_create(cell);
        lv_obj_set_width(inner, LV_PCT(100));
        lv_obj_set_height(inner, LV_PCT(100));
        lv_obj_set_align(inner, LV_ALIGN_CENTER);
        lv_obj_set_layout(inner, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(inner, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(inner, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_opa(inner, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(inner, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_column(inner, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(inner, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(inner, LV_OBJ_FLAG_CLICKABLE);  /* tap goes to cell */

        /* Swatch: fixed size on the left */
        lv_obj_t* swatch = lv_obj_create(inner);
        lv_obj_set_size(swatch, 45, 45);
        lv_obj_set_flex_grow(swatch, 0);
        lv_obj_set_style_bg_color(swatch, lv_color_hex(filaments[i].color_hex), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(swatch, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(swatch, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(swatch, lv_color_hex(0xE0E0E0), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(swatch, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(swatch, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(swatch, LV_OBJ_FLAG_CLICKABLE);  /* tap goes to cell */

        /* Label: takes remaining space to the right of swatch only */
        lv_obj_t* label = lv_label_create(inner);
        String text = filaments[i].brand + " " + filaments[i].name;
        lv_label_set_text(label, text.c_str());
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        lv_obj_set_flex_grow(label, 1);
        lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);  /* tap goes to cell */
    }
}

void ScreenLibrary::show()
{
    lv_screen_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}