#include <ui/screens/screen_spool_detail.h>
#include <inventory_manager.h>
#include <Arduino.h>

ScreenSpoolDetail screenSpoolDetail;

static lv_obj_t* createInfoRow(lv_obj_t* parent, const char* key, int y_pos) {
    lv_obj_t* lblKey = lv_label_create(parent);
    lv_label_set_text(lblKey, key);
    lv_obj_set_pos(lblKey, 8, y_pos);
    lv_obj_set_style_text_font(lblKey, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblKey, lv_color_hex(0x666666), 0);

    lv_obj_t* lblVal = lv_label_create(parent);
    lv_label_set_text(lblVal, "-");
    lv_obj_set_pos(lblVal, 240, y_pos);
    lv_obj_set_style_text_font(lblVal, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblVal, lv_color_black(), 0);

    return lblVal;
}

void ScreenSpoolDetail::init() {
    // Deferred init — screen is created lazily on first show()
    // to avoid LVGL object creation hang during boot.
    _initialized = false;
    screen = nullptr;
}

void ScreenSpoolDetail::_lazyInit() {
    if (_initialized) return;
    _initialized = true;

    screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);

    // Title bar
    btnBack = lv_btn_create(screen);
    lv_obj_set_size(btnBack, 70, 36);
    lv_obj_set_pos(btnBack, 10, 7);
    lv_obj_t* lblBack = lv_label_create(btnBack);
    lv_label_set_text(lblBack, LV_SYMBOL_LEFT);
    lv_obj_set_align(lblBack, LV_ALIGN_CENTER);

    lv_obj_t* titleLbl = lv_label_create(screen);
    lv_label_set_text(titleLbl, "Spool Detail");
    lv_obj_set_style_text_font(titleLbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(titleLbl, lv_color_black(), 0);
    lv_obj_set_pos(titleLbl, 340, 12);

    // Header card
    lv_obj_t* headerCard = lv_obj_create(screen);
    lv_obj_set_size(headerCard, 370, 60);
    lv_obj_set_pos(headerCard, 10, 55);
    lv_obj_set_style_bg_color(headerCard, lv_color_hex(0xF5F5F5), 0);
    lv_obj_set_style_radius(headerCard, 8, 0);
    lv_obj_set_style_border_width(headerCard, 0, 0);
    lv_obj_set_style_pad_all(headerCard, 8, 0);
    lv_obj_clear_flag(headerCard, LV_OBJ_FLAG_SCROLLABLE);

    colorSwatch = lv_obj_create(headerCard);
    lv_obj_set_size(colorSwatch, 44, 44);
    lv_obj_set_align(colorSwatch, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_bg_color(colorSwatch, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_bg_opa(colorSwatch, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(colorSwatch, 6, 0);
    lv_obj_set_style_border_width(colorSwatch, 1, 0);
    lv_obj_set_style_border_color(colorSwatch, lv_color_hex(0xCCCCCC), 0);
    lv_obj_clear_flag(colorSwatch, LV_OBJ_FLAG_SCROLLABLE);

    labelName = lv_label_create(headerCard);
    lv_label_set_text(labelName, "---");
    lv_label_set_long_mode(labelName, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(labelName, 280);
    lv_obj_set_pos(labelName, 60, 4);
    lv_obj_set_style_text_font(labelName, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(labelName, lv_color_black(), 0);

    labelMaterial = lv_label_create(headerCard);
    lv_label_set_text(labelMaterial, "");
    lv_obj_set_pos(labelMaterial, 60, 28);
    lv_obj_set_style_text_font(labelMaterial, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(labelMaterial, lv_color_hex(0x1565C0), 0);

    // Tag UID
    labelTagUID = lv_label_create(screen);
    lv_label_set_text(labelTagUID, "Tag: none");
    lv_obj_set_pos(labelTagUID, 14, 122);
    lv_obj_set_style_text_font(labelTagUID, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(labelTagUID, lv_color_hex(0x999999), 0);

    // Info box
    lv_obj_t* infoBox = lv_obj_create(screen);
    lv_obj_set_size(infoBox, 370, 160);
    lv_obj_set_pos(infoBox, 10, 140);
    lv_obj_set_style_bg_color(infoBox, lv_color_hex(0xFAFAFA), 0);
    lv_obj_set_style_radius(infoBox, 6, 0);
    lv_obj_set_style_border_width(infoBox, 1, 0);
    lv_obj_set_style_border_color(infoBox, lv_color_hex(0xE0E0E0), 0);
    lv_obj_clear_flag(infoBox, LV_OBJ_FLAG_SCROLLABLE);

    labelNozzleTemp = createInfoRow(infoBox, "Nozzle Temp", 6);
    labelBedTemp    = createInfoRow(infoBox, "Bed Temp", 28);
    labelSpeed      = createInfoRow(infoBox, "Print Speed", 50);
    labelFan        = createInfoRow(infoBox, "Fan", 72);
    labelDiameter   = createInfoRow(infoBox, "Diameter", 94);
    labelDensity    = createInfoRow(infoBox, "Density", 116);

    // Weight card
    lv_obj_t* weightCard = lv_obj_create(screen);
    lv_obj_set_size(weightCard, 370, 60);
    lv_obj_set_pos(weightCard, 400, 55);
    lv_obj_set_style_bg_color(weightCard, lv_color_hex(0xF5F5F5), 0);
    lv_obj_set_style_radius(weightCard, 8, 0);
    lv_obj_set_style_border_width(weightCard, 0, 0);
    lv_obj_set_style_pad_all(weightCard, 8, 0);
    lv_obj_clear_flag(weightCard, LV_OBJ_FLAG_SCROLLABLE);

    labelWeightText = lv_label_create(weightCard);
    lv_label_set_text(labelWeightText, "0g / 0g");
    lv_obj_set_align(labelWeightText, LV_ALIGN_TOP_LEFT);
    lv_obj_set_style_text_font(labelWeightText, &lv_font_montserrat_14, 0);

    labelWeightPct = lv_label_create(weightCard);
    lv_label_set_text(labelWeightPct, "0%");
    lv_obj_set_align(labelWeightPct, LV_ALIGN_TOP_RIGHT);
    lv_obj_set_style_text_font(labelWeightPct, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(labelWeightPct, lv_color_hex(0x4CAF50), 0);

    barWeight = lv_bar_create(weightCard);
    lv_obj_set_size(barWeight, LV_PCT(100), 14);
    lv_obj_set_align(barWeight, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_style_bg_color(barWeight, lv_color_hex(0xE0E0E0), LV_PART_MAIN);
    lv_obj_set_style_radius(barWeight, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(barWeight, lv_color_hex(0x4CAF50), LV_PART_INDICATOR);
    lv_obj_set_style_radius(barWeight, 4, LV_PART_INDICATOR);
    lv_bar_set_value(barWeight, 0, LV_ANIM_OFF);

    // History
    historyList = lv_obj_create(screen);
    lv_obj_set_size(historyList, 370, 80);
    lv_obj_set_pos(historyList, 400, 140);
    lv_obj_set_style_bg_color(historyList, lv_color_hex(0xFAFAFA), 0);
    lv_obj_set_style_radius(historyList, 6, 0);
    lv_obj_set_style_border_width(historyList, 1, 0);
    lv_obj_set_style_border_color(historyList, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_pad_all(historyList, 4, 0);
    lv_obj_set_scroll_dir(historyList, LV_DIR_VER);
    lv_obj_set_layout(historyList, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(historyList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(historyList, 2, 0);

    // Action buttons
    btnUpdateWeight = lv_btn_create(screen);
    lv_obj_set_size(btnUpdateWeight, 160, 38);
    lv_obj_set_pos(btnUpdateWeight, 400, 230);
    lv_obj_set_style_bg_color(btnUpdateWeight, lv_color_hex(0x2196F3), 0);
    lv_obj_t* luw = lv_label_create(btnUpdateWeight);
    lv_label_set_text(luw, "Update Weight");
    lv_obj_set_align(luw, LV_ALIGN_CENTER);
    lv_obj_set_style_text_font(luw, &lv_font_montserrat_12, 0);

    btnWriteTag = lv_btn_create(screen);
    lv_obj_set_size(btnWriteTag, 160, 38);
    lv_obj_set_pos(btnWriteTag, 570, 230);
    lv_obj_set_style_bg_color(btnWriteTag, lv_color_hex(0x4CAF50), 0);
    lv_obj_t* lwt = lv_label_create(btnWriteTag);
    lv_label_set_text(lwt, "Write to Tag");
    lv_obj_set_align(lwt, LV_ALIGN_CENTER);
    lv_obj_set_style_text_font(lwt, &lv_font_montserrat_12, 0);

    btnUnlinkTag = lv_btn_create(screen);
    lv_obj_set_size(btnUnlinkTag, 160, 38);
    lv_obj_set_pos(btnUnlinkTag, 400, 275);
    lv_obj_set_style_bg_color(btnUnlinkTag, lv_color_hex(0xFF9800), 0);
    lv_obj_t* lul = lv_label_create(btnUnlinkTag);
    lv_label_set_text(lul, "Unlink Tag");
    lv_obj_set_align(lul, LV_ALIGN_CENTER);
    lv_obj_set_style_text_font(lul, &lv_font_montserrat_12, 0);

    btnArchive = lv_btn_create(screen);
    lv_obj_set_size(btnArchive, 100, 38);
    lv_obj_set_pos(btnArchive, 400, 320);
    lv_obj_set_style_bg_color(btnArchive, lv_color_hex(0x9E9E9E), 0);
    lv_obj_t* lar = lv_label_create(btnArchive);
    lv_label_set_text(lar, "Archive");
    lv_obj_set_align(lar, LV_ALIGN_CENTER);
    lv_obj_set_style_text_font(lar, &lv_font_montserrat_12, 0);

    btnDelete = lv_btn_create(screen);
    lv_obj_set_size(btnDelete, 100, 38);
    lv_obj_set_pos(btnDelete, 510, 320);
    lv_obj_set_style_bg_color(btnDelete, lv_color_hex(0xF44336), 0);
    lv_obj_t* ldel = lv_label_create(btnDelete);
    lv_label_set_text(ldel, "Delete");
    lv_obj_set_align(ldel, LV_ALIGN_CENTER);
    lv_obj_set_style_text_font(ldel, &lv_font_montserrat_12, 0);

    // Weight modal on layer_top
    modalWeight = lv_obj_create(lv_display_get_layer_top(lv_display_get_default()));
    lv_obj_set_size(modalWeight, 300, 200);
    lv_obj_set_align(modalWeight, LV_ALIGN_CENTER);
    lv_obj_add_flag(modalWeight, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_pad_all(modalWeight, 16, 0);

    lv_obj_t* mtitle = lv_label_create(modalWeight);
    lv_label_set_text(mtitle, "Enter Weight (g)");
    lv_obj_set_align(mtitle, LV_ALIGN_TOP_MID);
    lv_obj_set_style_text_font(mtitle, &lv_font_montserrat_16, 0);

    spinboxWeight = lv_spinbox_create(modalWeight);
    lv_spinbox_set_range(spinboxWeight, 0, 5000);
    lv_spinbox_set_digit_format(spinboxWeight, 4, 0);
    lv_spinbox_set_step(spinboxWeight, 10);
    lv_obj_set_size(spinboxWeight, 160, 50);
    lv_obj_set_align(spinboxWeight, LV_ALIGN_CENTER);
    lv_obj_set_style_text_font(spinboxWeight, &lv_font_montserrat_24, 0);

    lv_obj_t* btnMinus = lv_btn_create(modalWeight);
    lv_obj_set_size(btnMinus, 50, 50);
    lv_obj_set_align(btnMinus, LV_ALIGN_CENTER);
    lv_obj_set_x(btnMinus, -110);
    lv_obj_t* lm = lv_label_create(btnMinus);
    lv_label_set_text(lm, LV_SYMBOL_MINUS);
    lv_obj_set_align(lm, LV_ALIGN_CENTER);
    lv_obj_add_event_cb(btnMinus, [](lv_event_t* e) {
        lv_spinbox_decrement(screenSpoolDetail.spinboxWeight);
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* btnPlus = lv_btn_create(modalWeight);
    lv_obj_set_size(btnPlus, 50, 50);
    lv_obj_set_align(btnPlus, LV_ALIGN_CENTER);
    lv_obj_set_x(btnPlus, 110);
    lv_obj_t* lp = lv_label_create(btnPlus);
    lv_label_set_text(lp, LV_SYMBOL_PLUS);
    lv_obj_set_align(lp, LV_ALIGN_CENTER);
    lv_obj_add_event_cb(btnPlus, [](lv_event_t* e) {
        lv_spinbox_increment(screenSpoolDetail.spinboxWeight);
    }, LV_EVENT_CLICKED, NULL);

    btnWeightOk = lv_btn_create(modalWeight);
    lv_obj_set_size(btnWeightOk, 100, 38);
    lv_obj_set_align(btnWeightOk, LV_ALIGN_BOTTOM_LEFT);
    lv_obj_set_style_bg_color(btnWeightOk, lv_color_hex(0x4CAF50), 0);
    lv_obj_t* lok = lv_label_create(btnWeightOk);
    lv_label_set_text(lok, "Save");
    lv_obj_set_align(lok, LV_ALIGN_CENTER);

    btnWeightCancel = lv_btn_create(modalWeight);
    lv_obj_set_size(btnWeightCancel, 100, 38);
    lv_obj_set_align(btnWeightCancel, LV_ALIGN_BOTTOM_RIGHT);
    lv_obj_t* lcan = lv_label_create(btnWeightCancel);
    lv_label_set_text(lcan, "Cancel");
    lv_obj_set_align(lcan, LV_ALIGN_CENTER);

}

void ScreenSpoolDetail::createWeightModal() {}

void ScreenSpoolDetail::loadSpool(const String& spool_id) {
    _lazyInit();
    currentSpoolId = spool_id;
    const SpoolRecord* rec = inventory.getSpoolById(spool_id);
    if (!rec) {
        lv_label_set_text(labelName, "Spool not found");
        return;
    }

    char name_buf[64];
    if (!rec->brand.isEmpty())
        snprintf(name_buf, sizeof(name_buf), "%s %s", rec->brand.c_str(), rec->name.c_str());
    else
        snprintf(name_buf, sizeof(name_buf), "%s", rec->name.c_str());
    lv_label_set_text(labelName, name_buf);
    lv_label_set_text(labelMaterial, rec->material_type.c_str());

    uint32_t color = strtoul(rec->color_hex.c_str(), nullptr, 16);
    lv_obj_set_style_bg_color(colorSwatch, lv_color_hex(color), 0);

    if (rec->tag_uid.isEmpty()) {
        lv_label_set_text(labelTagUID, "Tag: none (local only)");
        lv_obj_add_flag(btnUnlinkTag, LV_OBJ_FLAG_HIDDEN);
    } else {
        char uid_buf[32];
        snprintf(uid_buf, sizeof(uid_buf), "Tag: %s", rec->tag_uid.c_str());
        lv_label_set_text(labelTagUID, uid_buf);
        lv_obj_clear_flag(btnUnlinkTag, LV_OBJ_FLAG_HIDDEN);
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "%u-%u C", rec->nozzle_temp_min, rec->nozzle_temp_max);
    lv_label_set_text(labelNozzleTemp, buf);
    snprintf(buf, sizeof(buf), "%u-%u C", rec->bed_temp_min, rec->bed_temp_max);
    lv_label_set_text(labelBedTemp, buf);
    snprintf(buf, sizeof(buf), "%u-%u mm/s", rec->print_speed_min, rec->print_speed_max);
    lv_label_set_text(labelSpeed, buf);
    snprintf(buf, sizeof(buf), "%u%%", rec->fan_percent);
    lv_label_set_text(labelFan, buf);
    snprintf(buf, sizeof(buf), "%.2f mm", rec->diameter_um / 1000.0f);
    lv_label_set_text(labelDiameter, buf);
    snprintf(buf, sizeof(buf), "%.2f g/cm3", rec->density);
    lv_label_set_text(labelDensity, buf);

    uint32_t pct = (rec->initial_weight_g > 0)
        ? (100 * rec->current_weight_g / rec->initial_weight_g) : 0;
    if (pct > 100) pct = 100;
    snprintf(buf, sizeof(buf), "%ug / %ug", (unsigned)rec->current_weight_g, (unsigned)rec->initial_weight_g);
    lv_label_set_text(labelWeightText, buf);
    snprintf(buf, sizeof(buf), "%u%%", (unsigned)pct);
    lv_label_set_text(labelWeightPct, buf);
    lv_bar_set_value(barWeight, pct, LV_ANIM_OFF);

    lv_color_t bar_color;
    if (pct > 50) bar_color = lv_color_hex(0x4CAF50);
    else if (pct > 20) bar_color = lv_color_hex(0xFF9800);
    else bar_color = lv_color_hex(0xF44336);
    lv_obj_set_style_bg_color(barWeight, bar_color, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(labelWeightPct, bar_color, 0);

    lv_obj_clean(historyList);
    size_t histCount = rec->weight_history.size();
    size_t start = (histCount > 5) ? histCount - 5 : 0;
    if (histCount == 0) {
        lv_obj_t* empty = lv_label_create(historyList);
        lv_label_set_text(empty, "No history");
        lv_obj_set_style_text_color(empty, lv_color_hex(0x999999), 0);
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_12, 0);
    } else {
        for (size_t i = start; i < histCount; i++) {
            lv_obj_t* entry = lv_label_create(historyList);
            char hist_buf[32];
            uint32_t mins = rec->weight_history[i].timestamp / 60;
            snprintf(hist_buf, sizeof(hist_buf), "%ug  (t+%um)",
                     (unsigned)rec->weight_history[i].weight_g, (unsigned)mins);
            lv_label_set_text(entry, hist_buf);
            lv_obj_set_style_text_font(entry, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(entry, lv_color_hex(0x333333), 0);
        }
    }

    lv_spinbox_set_value(spinboxWeight, rec->current_weight_g);
}

void ScreenSpoolDetail::show() {
    _lazyInit();
    lv_screen_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}
