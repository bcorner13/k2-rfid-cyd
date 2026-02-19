#include <ui/screens/screen_inventory.h>
#include <inventory_manager.h>

ScreenInventory screenInventory;

void ScreenInventory::init() {
    screen = lv_obj_create(nullptr);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);

    // Title bar row
    lv_obj_t* titleBar = lv_obj_create(screen);
    lv_obj_set_size(titleBar, LV_PCT(100), 50);
    lv_obj_set_align(titleBar, LV_ALIGN_TOP_MID);
    lv_obj_set_style_bg_opa(titleBar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(titleBar, 0, 0);
    lv_obj_set_style_pad_all(titleBar, 0, 0);
    lv_obj_clear_flag(titleBar, LV_OBJ_FLAG_SCROLLABLE);

    // Back button
    btnBack = lv_btn_create(titleBar);
    lv_obj_set_size(btnBack, 70, 36);
    lv_obj_set_align(btnBack, LV_ALIGN_LEFT_MID);
    lv_obj_set_x(btnBack, 10);
    lv_obj_t* lblBack = lv_label_create(btnBack);
    lv_label_set_text(lblBack, LV_SYMBOL_LEFT);
    lv_obj_set_align(lblBack, LV_ALIGN_CENTER);

    // Title + count
    lv_obj_t* title = lv_label_create(titleBar);
    lv_label_set_text(title, "Inventory");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_set_align(title, LV_ALIGN_CENTER);

    labelCount = lv_label_create(titleBar);
    lv_label_set_text(labelCount, "");
    lv_obj_set_style_text_font(labelCount, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(labelCount, lv_color_hex(0x666666), 0);
    lv_obj_set_align(labelCount, LV_ALIGN_RIGHT_MID);
    lv_obj_set_x(labelCount, -10);

    // Action bar
    lv_obj_t* actionBar = lv_obj_create(screen);
    lv_obj_set_size(actionBar, LV_PCT(100), 50);
    lv_obj_set_align(actionBar, LV_ALIGN_TOP_MID);
    lv_obj_set_y(actionBar, 50);
    lv_obj_set_style_bg_opa(actionBar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(actionBar, 0, 0);
    lv_obj_set_layout(actionBar, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(actionBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(actionBar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(actionBar, 10, 0);
    lv_obj_clear_flag(actionBar, LV_OBJ_FLAG_SCROLLABLE);

    // Scan Tag button
    btnScanTag = lv_btn_create(actionBar);
    lv_obj_set_size(btnScanTag, 160, 40);
    lv_obj_set_style_bg_color(btnScanTag, lv_color_hex(0x4CAF50), 0);
    lv_obj_t* lblScan = lv_label_create(btnScanTag);
    lv_label_set_text(lblScan, LV_SYMBOL_REFRESH " Scan Tag");
    lv_obj_set_align(lblScan, LV_ALIGN_CENTER);

    // Add Custom button
    btnAddCustom = lv_btn_create(actionBar);
    lv_obj_set_size(btnAddCustom, 160, 40);
    lv_obj_set_style_bg_color(btnAddCustom, lv_color_hex(0x2196F3), 0);
    lv_obj_t* lblAdd = lv_label_create(btnAddCustom);
    lv_label_set_text(lblAdd, LV_SYMBOL_PLUS " Add Custom");
    lv_obj_set_align(lblAdd, LV_ALIGN_CENTER);

    // Scrollable list area
    list = lv_obj_create(screen);
    lv_obj_set_size(list, LV_PCT(96), 360);
    lv_obj_set_align(list, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_y(list, -10);
    lv_obj_set_layout(list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 4, 0);
    lv_obj_set_style_pad_all(list, 6, 0);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_ON);
    lv_obj_set_style_bg_color(list, lv_color_hex(0xF5F5F5), 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_radius(list, 8, 0);
}

void ScreenInventory::addSpoolRow(size_t index, const char* brand, const char* name,
                                   const char* material, uint32_t color_hex,
                                   uint32_t current_weight_g, uint32_t initial_weight_g,
                                   uint8_t status) {
    // Row container
    lv_obj_t* row = lv_obj_create(list);
    lv_obj_set_size(row, LV_PCT(100), 64);
    lv_obj_set_style_bg_color(row, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_pad_all(row, 8, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(row, (void*)index);

    // Color swatch (left)
    lv_obj_t* swatch = lv_obj_create(row);
    lv_obj_set_size(swatch, 40, 40);
    lv_obj_set_align(swatch, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_bg_color(swatch, lv_color_hex(color_hex), 0);
    lv_obj_set_style_bg_opa(swatch, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(swatch, 6, 0);
    lv_obj_set_style_border_width(swatch, 1, 0);
    lv_obj_set_style_border_color(swatch, lv_color_hex(0xCCCCCC), 0);
    lv_obj_clear_flag(swatch, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(swatch, LV_OBJ_FLAG_CLICKABLE);

    // Brand + Name label
    lv_obj_t* lblName = lv_label_create(row);
    char name_buf[64];
    if (brand[0] != '\0') {
        snprintf(name_buf, sizeof(name_buf), "%s %s", brand, name);
    } else {
        snprintf(name_buf, sizeof(name_buf), "%s", name);
    }
    lv_label_set_text(lblName, name_buf);
    lv_label_set_long_mode(lblName, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lblName, 280);
    lv_obj_set_pos(lblName, 56, 4);
    lv_obj_set_style_text_font(lblName, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblName, lv_color_black(), 0);
    lv_obj_clear_flag(lblName, LV_OBJ_FLAG_CLICKABLE);

    // Material badge
    lv_obj_t* badge = lv_obj_create(row);
    lv_obj_set_size(badge, LV_SIZE_CONTENT, 20);
    lv_obj_set_pos(badge, 56, 28);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0xE3F2FD), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(badge, 4, 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lv_obj_set_style_pad_hor(badge, 6, 0);
    lv_obj_set_style_pad_ver(badge, 2, 0);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* lblMat = lv_label_create(badge);
    lv_label_set_text(lblMat, material);
    lv_obj_set_style_text_font(lblMat, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblMat, lv_color_hex(0x1565C0), 0);
    lv_obj_set_align(lblMat, LV_ALIGN_CENTER);

    // Weight bar (right side)
    lv_obj_t* bar = lv_bar_create(row);
    lv_obj_set_size(bar, 120, 12);
    lv_obj_set_align(bar, LV_ALIGN_RIGHT_MID);
    lv_obj_set_x(bar, -60);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xE0E0E0), LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 4, LV_PART_MAIN);

    int32_t pct = (initial_weight_g > 0)
        ? (int32_t)(100 * current_weight_g / initial_weight_g)
        : 0;
    if (pct > 100) pct = 100;
    lv_bar_set_value(bar, pct, LV_ANIM_OFF);

    // Bar color based on percentage
    lv_color_t bar_color;
    if (pct > 50) bar_color = lv_color_hex(0x4CAF50);       // green
    else if (pct > 20) bar_color = lv_color_hex(0xFF9800);   // orange
    else bar_color = lv_color_hex(0xF44336);                  // red
    lv_obj_set_style_bg_color(bar, bar_color, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 4, LV_PART_INDICATOR);

    // Weight text
    lv_obj_t* lblWeight = lv_label_create(row);
    char wt_buf[16];
    snprintf(wt_buf, sizeof(wt_buf), "%ug", (unsigned)current_weight_g);
    lv_label_set_text(lblWeight, wt_buf);
    lv_obj_set_align(lblWeight, LV_ALIGN_RIGHT_MID);
    lv_obj_set_x(lblWeight, -8);
    lv_obj_set_style_text_font(lblWeight, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblWeight, lv_color_hex(0x666666), 0);
    lv_obj_clear_flag(lblWeight, LV_OBJ_FLAG_CLICKABLE);

    // Status indicator dot (top-right corner)
    lv_obj_t* dot = lv_obj_create(row);
    lv_obj_set_size(dot, 10, 10);
    lv_obj_set_align(dot, LV_ALIGN_TOP_RIGHT);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);

    // Status color: 0=ACTIVE(green), 1=EMPTY(red), 2=ARCHIVED(gray)
    lv_color_t dot_color;
    switch (status) {
        case 0: dot_color = lv_color_hex(0x4CAF50); break;  // ACTIVE
        case 1: dot_color = lv_color_hex(0xF44336); break;  // EMPTY
        case 2: dot_color = lv_color_hex(0x9E9E9E); break;  // ARCHIVED
        default: dot_color = lv_color_hex(0x9E9E9E); break;
    }
    lv_obj_set_style_bg_color(dot, dot_color, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
}

void ScreenInventory::populate() {
    lv_obj_clean(list);

    auto active = inventory.getAllActive();
    size_t count = active.size();

    char count_buf[16];
    snprintf(count_buf, sizeof(count_buf), "%u spools", (unsigned)count);
    lv_label_set_text(labelCount, count_buf);

    if (count == 0) {
        lv_obj_t* empty = lv_label_create(list);
        lv_label_set_text(empty, "No spools in inventory.\nTap 'Scan Tag' or 'Add Custom' to begin.");
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(empty, lv_color_hex(0x999999), 0);
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_16, 0);
        lv_obj_set_width(empty, LV_PCT(90));
        lv_obj_set_align(empty, LV_ALIGN_CENTER);
        return;
    }

    for (size_t i = 0; i < count; i++) {
        const SpoolRecord* rec = active[i];

        uint32_t color = strtoul(rec->color_hex.c_str(), nullptr, 16);

        addSpoolRow(i,
                    rec->brand.c_str(),
                    rec->name.c_str(),
                    rec->material_type.c_str(),
                    color,
                    rec->current_weight_g,
                    rec->initial_weight_g,
                    static_cast<uint8_t>(rec->status));
    }
}

void ScreenInventory::show() {
    populate();  // Refresh data each time screen is shown
    lv_screen_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}
