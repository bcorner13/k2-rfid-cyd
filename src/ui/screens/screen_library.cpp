#include <ui/screens/screen_library.h> // Updated include path
#include <filament_db.h> // Updated include path
#include <Arduino.h>

ScreenLibrary screenLibrary;

static constexpr uint8_t GRID_COLS = 4;
static constexpr uint8_t GRID_MAX_ROWS = 20;
static lv_coord_t col_dsc[GRID_COLS + 1];
static lv_coord_t row_dsc[GRID_MAX_ROWS + 1];

static const uint16_t BUTTON_HEIGHT = 60; // Added constant for button height

void ScreenLibrary::init()
{
    screen = lv_obj_create(nullptr);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "Select Filament");
    lv_obj_set_align(title, LV_ALIGN_TOP_MID); // Changed from lv_obj_align
    lv_obj_set_y(title, 10); // Set y offset

    // Back button
    btnBack = lv_btn_create(screen);
    lv_obj_set_size(btnBack, 80, 36);
    lv_obj_set_align(btnBack, LV_ALIGN_TOP_LEFT); // Changed from lv_obj_align
    lv_obj_set_x(btnBack, 10); // Set x offset
    lv_obj_set_y(btnBack, 8); // Set y offset

    lv_obj_t* lblBack = lv_label_create(btnBack);
    lv_label_set_text(lblBack, "< Back");
    lv_obj_set_align(lblBack, LV_ALIGN_CENTER); // Changed from lv_obj_center

    // Grid container
    grid = lv_obj_create(screen);
    lv_obj_set_size(grid, 710, 360);
    lv_obj_set_align(grid, LV_ALIGN_BOTTOM_MID); // Changed from lv_obj_align
    lv_obj_set_y(grid, -10); // Set y offset
    lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    // Populate and configure grid
    populate();
}


void ScreenLibrary::configureGrid(size_t filamentCount)
{
    uint16_t rows = (filamentCount + GRID_COLS - 1) / GRID_COLS;
    if (rows == 0) rows = 1;
    if (rows > GRID_MAX_ROWS) rows = GRID_MAX_ROWS;

    for (uint8_t i = 0; i < GRID_COLS; i++) {
        col_dsc[i] = LV_GRID_FR(1);
    }
    col_dsc[GRID_COLS] = LV_GRID_TEMPLATE_LAST;

    for (uint16_t i = 0; i < rows; i++) {
        row_dsc[i] = LV_GRID_FR(1);
    }
    row_dsc[rows] = LV_GRID_TEMPLATE_LAST;

    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
}


void ScreenLibrary::populate()
{
    lv_obj_clean(grid);

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
        lv_obj_set_height(cell, BUTTON_HEIGHT); // Set explicit height for the button

        lv_obj_t* swatch = lv_obj_create(cell);
        lv_obj_set_size(swatch, 28, 28);
        lv_obj_set_align(swatch, LV_ALIGN_TOP_LEFT); // Changed from lv_obj_align
        lv_obj_set_x(swatch, 8); // Set x offset
        lv_obj_set_y(swatch, 8); // Set y offset
        lv_obj_set_style_bg_color(
            swatch,
            lv_color_hex(filaments[i].color_hex),
            0
        );
        lv_obj_set_style_border_width(swatch, 1, 0);

        lv_obj_t* label = lv_label_create(cell);
        String text = filaments[i].brand + "\n" + filaments[i].name;
        lv_label_set_text(label, text.c_str());
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(label, lv_pct(90));
        lv_obj_set_align(label, LV_ALIGN_BOTTOM_MID); // Changed from lv_obj_align
        lv_obj_set_y(label, -6); // Set y offset
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    }
}

void ScreenLibrary::show()
{
    lv_screen_load(screen); // Changed from lv_scr_load
}