#include <ui/screens/screen_main.h>
#include <spool_data.h>
#include <ui/widgets/spool_widget.h>
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

    // --- Main Container (Grid Layout) ---
    lv_obj_t* mainContainer = lv_obj_create(screen);
    lv_obj_set_size(mainContainer, LV_PCT(100), LV_PCT(100)); // Use percentages for future-proofing
    lv_obj_set_align(mainContainer, LV_ALIGN_CENTER); // Changed from lv_obj_center
    lv_obj_set_style_border_width(mainContainer, 0, 0);
    lv_obj_set_style_bg_opa(mainContainer, 0, 0);
    lv_obj_set_layout(mainContainer, LV_LAYOUT_GRID);

    static lv_coord_t col_dsc[] = {300, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; // Left col 300px, Right col fills remaining space
    static lv_coord_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};      // Full height
    lv_obj_set_grid_dsc_array(mainContainer, col_dsc, row_dsc);

    // --- Left Panel (Spool) ---
    lv_obj_t* leftPanel = lv_obj_create(mainContainer);
    lv_obj_set_grid_cell(leftPanel, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_style_border_width(leftPanel, 0, 0);
    lv_obj_set_style_bg_opa(leftPanel, 0, 0);
    lv_obj_set_flex_flow(leftPanel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(leftPanel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    spoolWidget.create(leftPanel);
    lv_obj_set_size(spoolWidget.getContainer(), 280, 340);

    labelHexColor = lv_label_create(leftPanel);
    lv_label_set_text(labelHexColor, "#FFFFFF");
    lv_obj_set_style_text_font(labelHexColor, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(labelHexColor, lv_color_black(), 0);


    // --- Right Panel (Controls) ---
    lv_obj_t* rightPanel = lv_obj_create(mainContainer);
    lv_obj_set_grid_cell(rightPanel, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_style_border_width(rightPanel, 0, 0);
    lv_obj_set_style_bg_opa(rightPanel, 0, 0);
    lv_obj_set_flex_flow(rightPanel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(rightPanel, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(rightPanel, 20, 0);

    // Dropdowns: options from material_database.json (unique brands and material types)
    ddBrand = lv_dropdown_create(rightPanel);
    {
        String opts = filamentDB.getBrandOptionsForDropdown();
        lv_dropdown_set_options(ddBrand, opts.isEmpty() ? "Generic" : opts.c_str());
    }
    lv_obj_set_width(ddBrand, 300);
    lv_obj_set_style_text_font(ddBrand, &lv_font_montserrat_20, 0);

    ddType = lv_dropdown_create(rightPanel);
    {
        String opts = filamentDB.getMaterialTypeOptionsForDropdown();
        lv_dropdown_set_options(ddType, opts.isEmpty() ? "PLA" : opts.c_str());
    }
    lv_obj_set_width(ddType, 300);
    lv_obj_set_style_text_font(ddType, &lv_font_montserrat_20, 0);

    // Slider Container
    lv_obj_t* sliderCont = lv_obj_create(rightPanel);
    lv_obj_set_size(sliderCont, 350, 80);
    lv_obj_set_style_bg_opa(sliderCont, 0, 0);
    lv_obj_set_style_border_width(sliderCont, 0, 0);

    sliderWeight = lv_slider_create(sliderCont);
    lv_obj_set_width(sliderWeight, 300);
    lv_obj_set_align(sliderWeight, LV_ALIGN_TOP_MID);
    lv_obj_set_y(sliderWeight, 10);
    lv_slider_set_range(sliderWeight, 0, 1000);
    lv_slider_set_mode(sliderWeight, LV_SLIDER_MODE_NORMAL);
    lv_slider_set_orientation(sliderWeight, LV_SLIDER_ORIENTATION_HORIZONTAL);

    labelWeight = lv_label_create(sliderCont);
    lv_label_set_text(labelWeight, "1000g");
    lv_obj_set_align(labelWeight, LV_ALIGN_BOTTOM_MID); // Changed from lv_obj_align
    lv_obj_set_y(labelWeight, 0); // Set y offset
    lv_obj_set_style_text_font(labelWeight, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(labelWeight, lv_color_black(), 0);

    // Buttons Container
    lv_obj_t* btnCont = lv_obj_create(rightPanel);
    lv_obj_set_size(btnCont, 400, 80);
    lv_obj_set_style_bg_opa(btnCont, 0, 0);
    lv_obj_set_style_border_width(btnCont, 0, 0);
    lv_obj_set_flex_flow(btnCont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnCont, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    btnWrite = lv_btn_create(btnCont);
    lv_obj_set_size(btnWrite, 120, 60);
    lv_obj_t* lWrite = lv_label_create(btnWrite);
    lv_label_set_text(lWrite, "WRITE");
    lv_obj_set_align(lWrite, LV_ALIGN_CENTER); // Changed from lv_obj_center

    btnLibrary = lv_btn_create(btnCont);
    lv_obj_set_size(btnLibrary, 60, 60);
    lv_obj_t* lLib = lv_label_create(btnLibrary);
    lv_label_set_text(lLib, LV_SYMBOL_LIST);
    lv_obj_set_align(lLib, LV_ALIGN_CENTER); // Changed from lv_obj_center

    btnSettings = lv_btn_create(btnCont);
    lv_obj_set_size(btnSettings, 60, 60);
    lv_obj_t* lSet = lv_label_create(btnSettings);
    lv_label_set_text(lSet, LV_SYMBOL_SETTINGS);
    lv_obj_set_align(lSet, LV_ALIGN_CENTER); // Changed from lv_obj_center
}

void ScreenMain::show() {
    lv_screen_load(screen); // Changed from lv_scr_load
}

void ScreenMain::update(const SpoolData& data) {
    // Update dropdowns first (match by exact or prefix so e.g. "CR-PL" from tag selects "CR-PLA")
    lv_dropdown_set_selected(ddBrand, find_dropdown_option_index(ddBrand, data.getBrand()));
    lv_dropdown_set_selected(ddType, find_dropdown_option_index(ddType, data.getType()));

    // Use full dropdown option text for spool label so "PLA-Silk" / "CR-PLA" show correctly
    char typeBuf[64];
    get_dropdown_option_at(ddType, lv_dropdown_get_selected(ddType), typeBuf, sizeof(typeBuf));
    const char* displayType = (typeBuf[0] != '\0') ? typeBuf : data.getType().c_str();

    spoolWidget.update(displayType, data.getColorHex(), data.getWeight());
    lv_label_set_text(labelHexColor, data.getColorName().c_str());
    lv_slider_set_value(sliderWeight, data.getWeight(), LV_ANIM_ON);
    lv_label_set_text_fmt(labelWeight, "%dg", data.getWeight());

    // Explicitly invalidate the updated objects to force a redraw
    lv_obj_invalidate(spoolWidget.getContainer());
    lv_obj_invalidate(labelHexColor);
    lv_obj_invalidate(labelWeight);
    lv_obj_invalidate(ddBrand);
    lv_obj_invalidate(ddType);
}