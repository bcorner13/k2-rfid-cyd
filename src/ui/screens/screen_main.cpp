#include <ui/screens/screen_main.h>
#include <spool_data.h>
#include <filament_db.h>
#include <cstring>

// Return the nth option (newline-separated) into buf for display
static void get_dropdown_option_at(lv_obj_t* dropdown, uint16_t index, char* buf, size_t buf_size) {
    const char* opts = lv_dropdown_get_options(dropdown);
    if (!opts || buf_size == 0) { buf[0] = '\0'; return; }
    for (uint16_t i = 0; i <= index; i++) {
        const char* start = opts;
        while (*opts != '\0' && *opts != '\n') opts++;
        if (i == index) {
            size_t len = (size_t)(opts - start);
            if (len >= buf_size) len = buf_size - 1;
            memcpy(buf, start, len);
            buf[len] = '\0';
            return;
        }
        if (*opts == '\0') break;
        opts++;
    }
    buf[0] = '\0';
}

// Find dropdown index: exact match, or first option that starts with target (for truncated tag type e.g. "CR-PL" -> "CR-PLA")
static uint16_t find_dropdown_option_index(lv_obj_t* dropdown, const std::string& target_option) {
    const char* options_cstr = lv_dropdown_get_options(dropdown);
    if (!options_cstr) return 0;
    std::string options_str(options_cstr);
    size_t start = 0;
    uint16_t index = 0;
    while (start < options_str.length()) {
        size_t end = options_str.find('\n', start);
        std::string current_option = options_str.substr(start, (end == std::string::npos) ? std::string::npos : end - start);
        size_t first = current_option.find_first_not_of(' ');
        size_t last = current_option.find_last_not_of(' ');
        if (std::string::npos == first) current_option = "";
        else current_option = current_option.substr(first, last - first + 1);

        if (current_option == target_option) return index;
        if (!target_option.empty() && current_option.length() >= target_option.length() &&
            current_option.compare(0, target_option.length(), target_option) == 0) return index;
        if (end == std::string::npos) break;
        start = end + 1;
        index++;
    }
    return 0;
}


void ScreenMain::init() {
    screen = lv_obj_create(NULL);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);

    /* --- Grid: content row | grey line | bottom buttons (left/right/bottom = 3 regions) --- */
    static lv_coord_t row_dsc[] = {LV_GRID_FR(1), 4, 100, LV_GRID_TEMPLATE_LAST};
    static lv_coord_t col_dsc[] = {LV_GRID_FR(1), 4, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_set_layout(screen, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(screen, col_dsc, row_dsc);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_pad_row(screen, 0, 0);
    lv_obj_set_style_pad_column(screen, 0, 0);

    /* --- Grey horizontal line above buttons --- */
    lv_obj_t* lineH = lv_obj_create(screen);
    lv_obj_set_size(lineH, LV_PCT(100), 4);
    lv_obj_set_style_bg_color(lineH, lv_color_hex(0x808080), 0);
    lv_obj_set_style_border_width(lineH, 0, 0);
    lv_obj_set_style_radius(lineH, 0, 0);
    lv_obj_set_grid_cell(lineH, LV_GRID_ALIGN_STRETCH, 0, 3, LV_GRID_ALIGN_STRETCH, 1, 1);

    /* --- Left area: color block (tap opens color picker) --- */
    lv_obj_t* leftPanel = lv_obj_create(screen);
    lv_obj_set_style_bg_opa(leftPanel, 0, 0);
    lv_obj_set_style_border_width(leftPanel, 0, 0);
    lv_obj_set_grid_cell(leftPanel, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_flex_flow(leftPanel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(leftPanel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    colorBlock = lv_obj_create(leftPanel);
    lv_obj_set_size(colorBlock, 200, 160);
    lv_obj_set_style_bg_color(colorBlock, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(colorBlock, 2, 0);
    lv_obj_set_style_border_color(colorBlock, lv_color_hex(0x404040), 0);
    lv_obj_set_style_radius(colorBlock, 8, 0);
    lv_obj_add_flag(colorBlock, LV_OBJ_FLAG_CLICKABLE);

    labelHexColor = lv_label_create(colorBlock);
    lv_label_set_text(labelHexColor, "#FFFFFF");
    lv_obj_set_style_text_font(labelHexColor, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(labelHexColor, lv_color_black(), 0);
    lv_obj_set_align(labelHexColor, LV_ALIGN_CENTER);

    /* --- Vertical grey divider (content area only, not bottom) --- */
    lv_obj_t* lineV = lv_obj_create(screen);
    lv_obj_set_size(lineV, 4, LV_PCT(100));
    lv_obj_set_style_bg_color(lineV, lv_color_hex(0x808080), 0);
    lv_obj_set_style_border_width(lineV, 0, 0);
    lv_obj_set_style_radius(lineV, 0, 0);
    lv_obj_set_grid_cell(lineV, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);

    /* --- Right area: brand, type, volume --- */
    lv_obj_t* rightPanel = lv_obj_create(screen);
    lv_obj_set_style_bg_opa(rightPanel, 0, 0);
    lv_obj_set_style_border_width(rightPanel, 0, 0);
    lv_obj_set_grid_cell(rightPanel, LV_GRID_ALIGN_STRETCH, 2, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_flex_flow(rightPanel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(rightPanel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(rightPanel, 20, 0);

    ddBrand = lv_dropdown_create(rightPanel);
    {
        String opts = filamentDB.getBrandOptionsForDropdown();
        lv_dropdown_set_options(ddBrand, opts.isEmpty() ? "Generic" : opts.c_str());
    }
    lv_obj_set_width(ddBrand, LV_PCT(100));
    lv_obj_set_style_text_font(ddBrand, &lv_font_montserrat_20, 0);

    ddType = lv_dropdown_create(rightPanel);
    {
        String opts = filamentDB.getMaterialTypeOptionsForDropdown();
        lv_dropdown_set_options(ddType, opts.isEmpty() ? "PLA" : opts.c_str());
    }
    lv_obj_set_width(ddType, LV_PCT(100));
    lv_obj_set_style_text_font(ddType, &lv_font_montserrat_20, 0);

    lv_obj_t* sliderCont = lv_obj_create(rightPanel);
    lv_obj_set_width(sliderCont, LV_PCT(100));
    lv_obj_set_height(sliderCont, 56);
    lv_obj_set_style_bg_opa(sliderCont, 0, 0);
    lv_obj_set_style_border_width(sliderCont, 0, 0);

    sliderWeight = lv_slider_create(sliderCont);
    lv_obj_set_width(sliderWeight, lv_pct(100));
    lv_obj_set_height(sliderWeight, 32);
    lv_obj_set_align(sliderWeight, LV_ALIGN_TOP_MID);
    lv_obj_set_y(sliderWeight, 0);
    lv_slider_set_range(sliderWeight, 0, 1000);
    lv_slider_set_mode(sliderWeight, LV_SLIDER_MODE_NORMAL);
    lv_slider_set_orientation(sliderWeight, LV_SLIDER_ORIENTATION_HORIZONTAL);

    labelWeight = lv_label_create(sliderCont);
    lv_label_set_text(labelWeight, "1000g");
    lv_obj_set_align(labelWeight, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_y(labelWeight, 0);
    lv_obj_set_style_text_font(labelWeight, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(labelWeight, lv_color_black(), 0);

    /* --- Bottom region: status + Read, Write, Library, Settings (all visible) --- */
    lv_obj_t* bottomArea = lv_obj_create(screen);
    lv_obj_set_style_bg_color(bottomArea, lv_color_hex(0xE8E8E8), 0);
    lv_obj_set_style_border_width(bottomArea, 0, 0);
    lv_obj_set_grid_cell(bottomArea, LV_GRID_ALIGN_STRETCH, 0, 3, LV_GRID_ALIGN_STRETCH, 2, 1);
    lv_obj_set_flex_flow(bottomArea, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(bottomArea, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(bottomArea, 8, 0);

    labelWriteStatus = lv_label_create(bottomArea);
    lv_label_set_text(labelWriteStatus, "Ready");
    lv_obj_set_style_text_font(labelWriteStatus, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(labelWriteStatus, lv_color_hex(0x404040), 0);

    lv_obj_t* btnCont = lv_obj_create(bottomArea);
    lv_obj_set_style_bg_opa(btnCont, 0, 0);
    lv_obj_set_style_border_width(btnCont, 0, 0);
    lv_obj_set_flex_flow(btnCont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnCont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btnCont, 4, 0);
    lv_obj_set_width(btnCont, 260);

    btnReadRfid = lv_btn_create(btnCont);
    lv_obj_set_size(btnReadRfid, 62, 46);
    lv_obj_t* lRead = lv_label_create(btnReadRfid);
    lv_label_set_text(lRead, "READ");
    lv_obj_set_align(lRead, LV_ALIGN_CENTER);
    lv_obj_set_style_text_font(lRead, &lv_font_montserrat_14, 0);

    btnWrite = lv_btn_create(btnCont);
    lv_obj_set_size(btnWrite, 62, 46);
    lv_obj_t* lWrite = lv_label_create(btnWrite);
    lv_label_set_text(lWrite, "WRITE");
    lv_obj_set_align(lWrite, LV_ALIGN_CENTER);
    lv_obj_set_style_text_font(lWrite, &lv_font_montserrat_14, 0);

    btnLibrary = lv_btn_create(btnCont);
    lv_obj_set_size(btnLibrary, 44, 46);
    lv_obj_t* lLib = lv_label_create(btnLibrary);
    lv_label_set_text(lLib, LV_SYMBOL_LIST);
    lv_obj_set_align(lLib, LV_ALIGN_CENTER);

    btnSettings = lv_btn_create(btnCont);
    lv_obj_set_size(btnSettings, 44, 46);
    lv_obj_t* lSet = lv_label_create(btnSettings);
    lv_label_set_text(lSet, LV_SYMBOL_SETTINGS);
    lv_obj_set_align(lSet, LV_ALIGN_CENTER);
}

void ScreenMain::show() {
    lv_screen_load(screen);
}

void ScreenMain::setWriteStatus(const char* status) {
    lv_label_set_text(labelWriteStatus, status);
    lv_obj_invalidate(labelWriteStatus);
}

void ScreenMain::update(const SpoolData& data) {
    lv_dropdown_set_selected(ddBrand, find_dropdown_option_index(ddBrand, data.getBrand()));
    lv_dropdown_set_selected(ddType, find_dropdown_option_index(ddType, data.getType()));

    lv_obj_set_style_bg_color(colorBlock, lv_color_hex(data.getColorHex()), 0);
    lv_label_set_text(labelHexColor, data.getColorName().c_str());
    lv_slider_set_value(sliderWeight, data.getWeight(), LV_ANIM_ON);
    lv_label_set_text_fmt(labelWeight, "%dg", data.getWeight());

    lv_obj_invalidate(colorBlock);
    lv_obj_invalidate(labelHexColor);
    lv_obj_invalidate(labelWeight);
    lv_obj_invalidate(ddBrand);
    lv_obj_invalidate(ddType);
}