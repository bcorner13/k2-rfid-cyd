#include <ui/screens/screen_custom_entry.h>
#include <ui/color_palette.h>
#include <filament_db.h>

ScreenCustomEntry screenCustomEntry;

static const char* STEP_TITLES[] = {
    "Step 1: Identity",
    "Step 2: Appearance",
    "Step 3: Temperatures & Weight",
    "Step 4: Speed & Fan",
    "Step 5: Review & Save"
};

// Helper: create a labeled spinbox row and return the spinbox
static lv_obj_t* createSpinRow(lv_obj_t* parent, const char* label,
                                int32_t min_val, int32_t max_val,
                                int32_t initial, uint8_t digits, int32_t step) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), 44);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl = lv_label_create(row);
    lv_label_set_text(lbl, label);
    lv_obj_set_align(lbl, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);

    lv_obj_t* spin = lv_spinbox_create(row);
    lv_spinbox_set_range(spin, min_val, max_val);
    lv_spinbox_set_digit_format(spin, digits, 0);
    lv_spinbox_set_step(spin, step);
    lv_spinbox_set_value(spin, initial);
    lv_obj_set_size(spin, 100, 36);
    lv_obj_set_align(spin, LV_ALIGN_RIGHT_MID);
    lv_obj_set_style_text_font(spin, &lv_font_montserrat_14, 0);

    // +/- buttons
    lv_obj_t* btnMinus = lv_btn_create(row);
    lv_obj_set_size(btnMinus, 32, 32);
    lv_obj_set_align(btnMinus, LV_ALIGN_RIGHT_MID);
    lv_obj_set_x(btnMinus, -108);
    lv_obj_t* lm = lv_label_create(btnMinus);
    lv_label_set_text(lm, LV_SYMBOL_MINUS);
    lv_obj_set_align(lm, LV_ALIGN_CENTER);
    lv_obj_set_style_text_font(lm, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(btnMinus, [](lv_event_t* e) {
        lv_obj_t* s = (lv_obj_t*)lv_event_get_user_data(e);
        lv_spinbox_decrement(s);
    }, LV_EVENT_CLICKED, spin);

    lv_obj_t* btnPlus = lv_btn_create(row);
    lv_obj_set_size(btnPlus, 32, 32);
    lv_obj_set_align(btnPlus, LV_ALIGN_RIGHT_MID);
    lv_obj_set_x(btnPlus, 36);
    lv_obj_t* lp = lv_label_create(btnPlus);
    lv_label_set_text(lp, LV_SYMBOL_PLUS);
    lv_obj_set_align(lp, LV_ALIGN_CENTER);
    lv_obj_set_style_text_font(lp, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(btnPlus, [](lv_event_t* e) {
        lv_obj_t* s = (lv_obj_t*)lv_event_get_user_data(e);
        lv_spinbox_increment(s);
    }, LV_EVENT_CLICKED, spin);

    return spin;
}

// Helper: create a review row (key: value) and return the value label
static lv_obj_t* createReviewRow(lv_obj_t* parent, const char* key) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), 22);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lk = lv_label_create(row);
    lv_label_set_text(lk, key);
    lv_obj_set_align(lk, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_font(lk, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lk, lv_color_hex(0x666666), 0);

    lv_obj_t* lv = lv_label_create(row);
    lv_label_set_text(lv, "-");
    lv_obj_set_align(lv, LV_ALIGN_RIGHT_MID);
    lv_obj_set_style_text_font(lv, &lv_font_montserrat_12, 0);

    return lv;
}

void ScreenCustomEntry::init() {
    _initialized = false;
    screen = nullptr;
}

void ScreenCustomEntry::_lazyInit() {
    if (_initialized) return;
    _initialized = true;
    screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    // ---- Title bar ----
    lv_obj_t* titleBar = lv_obj_create(screen);
    lv_obj_set_size(titleBar, LV_PCT(100), 50);
    lv_obj_set_style_bg_opa(titleBar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(titleBar, 0, 0);
    lv_obj_clear_flag(titleBar, LV_OBJ_FLAG_SCROLLABLE);

    btnCancel = lv_btn_create(titleBar);
    lv_obj_set_size(btnCancel, 80, 36);
    lv_obj_set_align(btnCancel, LV_ALIGN_LEFT_MID);
    lv_obj_set_x(btnCancel, 10);
    lv_obj_set_style_bg_color(btnCancel, lv_color_hex(0x9E9E9E), 0);
    lv_obj_t* lc = lv_label_create(btnCancel);
    lv_label_set_text(lc, "Cancel");
    lv_obj_set_align(lc, LV_ALIGN_CENTER);
    lv_obj_set_style_text_font(lc, &lv_font_montserrat_12, 0);

    labelStepTitle = lv_label_create(titleBar);
    lv_label_set_text(labelStepTitle, STEP_TITLES[0]);
    lv_obj_set_style_text_font(labelStepTitle, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(labelStepTitle, lv_color_black(), 0);
    lv_obj_set_align(labelStepTitle, LV_ALIGN_CENTER);

    // Step indicator dots
    stepIndicator = lv_obj_create(titleBar);
    lv_obj_set_size(stepIndicator, 120, 20);
    lv_obj_set_align(stepIndicator, LV_ALIGN_RIGHT_MID);
    lv_obj_set_x(stepIndicator, -10);
    lv_obj_set_style_bg_opa(stepIndicator, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(stepIndicator, 0, 0);
    lv_obj_set_style_pad_all(stepIndicator, 0, 0);
    lv_obj_clear_flag(stepIndicator, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(stepIndicator, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(stepIndicator, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(stepIndicator, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(stepIndicator, 8, 0);

    for (int i = 0; i < STEP_COUNT; i++) {
        lv_obj_t* dot = lv_obj_create(stepIndicator);
        lv_obj_set_size(dot, 12, 12);
        lv_obj_set_style_radius(dot, 6, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    }

    // ---- Content area (holds step panels) ----
    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_size(content, LV_PCT(96), 340);
    lv_obj_set_align(content, LV_ALIGN_TOP_MID);
    lv_obj_set_y(content, 52);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    // Create step panels (all same size, show/hide)
    for (int i = 0; i < STEP_COUNT; i++) {
        stepPanels[i] = lv_obj_create(content);
        lv_obj_set_size(stepPanels[i], LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_opa(stepPanels[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(stepPanels[i], 0, 0);
        lv_obj_set_style_pad_all(stepPanels[i], 8, 0);
        lv_obj_set_scroll_dir(stepPanels[i], LV_DIR_VER);
        lv_obj_set_layout(stepPanels[i], LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(stepPanels[i], LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(stepPanels[i], 8, 0);
        lv_obj_add_flag(stepPanels[i], LV_OBJ_FLAG_HIDDEN);
    }

    createStep1();
    createStep2();
    createStep3();
    createStep4();
    createStep5();

    // ---- Bottom navigation bar ----
    lv_obj_t* navBar = lv_obj_create(screen);
    lv_obj_set_size(navBar, LV_PCT(96), 50);
    lv_obj_set_align(navBar, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_y(navBar, -10);
    lv_obj_set_style_bg_opa(navBar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(navBar, 0, 0);
    lv_obj_clear_flag(navBar, LV_OBJ_FLAG_SCROLLABLE);

    btnPrev = lv_btn_create(navBar);
    lv_obj_set_size(btnPrev, 120, 40);
    lv_obj_set_align(btnPrev, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_bg_color(btnPrev, lv_color_hex(0x9E9E9E), 0);
    lv_obj_t* lprev = lv_label_create(btnPrev);
    lv_label_set_text(lprev, LV_SYMBOL_LEFT " Back");
    lv_obj_set_align(lprev, LV_ALIGN_CENTER);

    btnNext = lv_btn_create(navBar);
    lv_obj_set_size(btnNext, 120, 40);
    lv_obj_set_align(btnNext, LV_ALIGN_RIGHT_MID);
    lv_obj_set_style_bg_color(btnNext, lv_color_hex(0x2196F3), 0);
    lv_obj_t* lnext = lv_label_create(btnNext);
    lv_label_set_text(lnext, "Next " LV_SYMBOL_RIGHT);
    lv_obj_set_align(lnext, LV_ALIGN_CENTER);

    // Save buttons (hidden until step 5)
    btnSave = lv_btn_create(navBar);
    lv_obj_set_size(btnSave, 160, 40);
    lv_obj_set_align(btnSave, LV_ALIGN_CENTER);
    lv_obj_set_x(btnSave, -90);
    lv_obj_set_style_bg_color(btnSave, lv_color_hex(0x4CAF50), 0);
    lv_obj_t* lsave = lv_label_create(btnSave);
    lv_label_set_text(lsave, "Save Locally");
    lv_obj_set_align(lsave, LV_ALIGN_CENTER);
    lv_obj_add_flag(btnSave, LV_OBJ_FLAG_HIDDEN);

    btnSaveWrite = lv_btn_create(navBar);
    lv_obj_set_size(btnSaveWrite, 180, 40);
    lv_obj_set_align(btnSaveWrite, LV_ALIGN_CENTER);
    lv_obj_set_x(btnSaveWrite, 90);
    lv_obj_set_style_bg_color(btnSaveWrite, lv_color_hex(0x2196F3), 0);
    lv_obj_t* lsw = lv_label_create(btnSaveWrite);
    lv_label_set_text(lsw, "Save + Write Tag");
    lv_obj_set_align(lsw, LV_ALIGN_CENTER);
    lv_obj_add_flag(btnSaveWrite, LV_OBJ_FLAG_HIDDEN);

    createColorPicker();

    goToStep(0);
}

void ScreenCustomEntry::createStep1() {
    lv_obj_t* p = stepPanels[0];

    lv_obj_t* lblBrand = lv_label_create(p);
    lv_label_set_text(lblBrand, "Brand");
    lv_obj_set_style_text_font(lblBrand, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblBrand, lv_color_hex(0x666666), 0);

    taBrand = lv_textarea_create(p);
    lv_textarea_set_one_line(taBrand, true);
    lv_textarea_set_max_length(taBrand, 32);
    lv_textarea_set_placeholder_text(taBrand, "e.g. Creality, Bambu, eSUN...");
    lv_obj_set_size(taBrand, LV_PCT(100), 44);

    lv_obj_t* lblName = lv_label_create(p);
    lv_label_set_text(lblName, "Filament Name");
    lv_obj_set_style_text_font(lblName, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblName, lv_color_hex(0x666666), 0);

    taName = lv_textarea_create(p);
    lv_textarea_set_one_line(taName, true);
    lv_textarea_set_max_length(taName, 48);
    lv_textarea_set_placeholder_text(taName, "e.g. Hyper PLA, PETG Pro...");
    lv_obj_set_size(taName, LV_PCT(100), 44);

    lv_obj_t* lblMat = lv_label_create(p);
    lv_label_set_text(lblMat, "Material Type");
    lv_obj_set_style_text_font(lblMat, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblMat, lv_color_hex(0x666666), 0);

    ddMaterial = lv_dropdown_create(p);
    lv_obj_set_size(ddMaterial, LV_PCT(100), 44);
    String matOpts = filamentDB.getMaterialTypeOptionsForDropdown();
    if (matOpts.isEmpty()) {
        matOpts = "PLA\nPETG\nABS\nTPU\nASA\nPA\nPC";
    }
    lv_dropdown_set_options(ddMaterial, matOpts.c_str());
}

void ScreenCustomEntry::createStep2() {
    lv_obj_t* p = stepPanels[1];

    lv_obj_t* lblColor = lv_label_create(p);
    lv_label_set_text(lblColor, "Color");
    lv_obj_set_style_text_font(lblColor, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblColor, lv_color_hex(0x666666), 0);

    // Color swatch (tap to open picker)
    lv_obj_t* colorRow = lv_obj_create(p);
    lv_obj_set_size(colorRow, LV_PCT(100), 56);
    lv_obj_set_style_bg_opa(colorRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(colorRow, 0, 0);
    lv_obj_set_style_pad_all(colorRow, 0, 0);
    lv_obj_clear_flag(colorRow, LV_OBJ_FLAG_SCROLLABLE);

    colorSwatch = lv_obj_create(colorRow);
    lv_obj_set_size(colorSwatch, 56, 56);
    lv_obj_set_align(colorSwatch, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_bg_color(colorSwatch, lv_color_hex(selectedColor), 0);
    lv_obj_set_style_bg_opa(colorSwatch, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(colorSwatch, 8, 0);
    lv_obj_set_style_border_width(colorSwatch, 2, 0);
    lv_obj_set_style_border_color(colorSwatch, lv_color_hex(0xCCCCCC), 0);
    lv_obj_clear_flag(colorSwatch, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(colorSwatch, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* lblTap = lv_label_create(colorRow);
    lv_label_set_text(lblTap, "Tap swatch to change color");
    lv_obj_set_align(lblTap, LV_ALIGN_LEFT_MID);
    lv_obj_set_x(lblTap, 70);
    lv_obj_set_style_text_font(lblTap, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblTap, lv_color_hex(0x999999), 0);

    // Tap swatch opens color picker
    lv_obj_add_event_cb(colorSwatch, [](lv_event_t* e) {
        lv_obj_clear_flag(screenCustomEntry.modalColor, LV_OBJ_FLAG_HIDDEN);
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* lblDia = lv_label_create(p);
    lv_label_set_text(lblDia, "Diameter");
    lv_obj_set_style_text_font(lblDia, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblDia, lv_color_hex(0x666666), 0);

    ddDiameter = lv_dropdown_create(p);
    lv_obj_set_size(ddDiameter, LV_PCT(100), 44);
    lv_dropdown_set_options(ddDiameter, "1.75 mm\n2.85 mm");
}

void ScreenCustomEntry::createStep3() {
    lv_obj_t* p = stepPanels[2];

    spinWeight    = createSpinRow(p, "Weight (g)",      50, 5000, 1000, 4, 50);
    spinNozzleMin = createSpinRow(p, "Nozzle Min (C)",   0,  500,  190, 3, 5);
    spinNozzleMax = createSpinRow(p, "Nozzle Max (C)",   0,  500,  220, 3, 5);
    spinBedMin    = createSpinRow(p, "Bed Min (C)",      0,  150,   50, 3, 5);
    spinBedMax    = createSpinRow(p, "Bed Max (C)",      0,  150,   70, 3, 5);
}

void ScreenCustomEntry::createStep4() {
    lv_obj_t* p = stepPanels[3];

    spinSpeedMin = createSpinRow(p, "Speed Min (mm/s)", 0, 2000, 40, 4, 10);
    spinSpeedMax = createSpinRow(p, "Speed Max (mm/s)", 0, 2000, 100, 4, 10);

    lv_obj_t* fanRow = lv_obj_create(p);
    lv_obj_set_size(fanRow, LV_PCT(100), 50);
    lv_obj_set_style_bg_opa(fanRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(fanRow, 0, 0);
    lv_obj_set_style_pad_all(fanRow, 0, 0);
    lv_obj_clear_flag(fanRow, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lblFan = lv_label_create(fanRow);
    lv_label_set_text(lblFan, "Fan");
    lv_obj_set_align(lblFan, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_font(lblFan, &lv_font_montserrat_12, 0);

    sliderFan = lv_slider_create(fanRow);
    lv_obj_set_size(sliderFan, 200, 20);
    lv_obj_set_align(sliderFan, LV_ALIGN_CENTER);
    lv_slider_set_range(sliderFan, 0, 100);
    lv_slider_set_value(sliderFan, 100, LV_ANIM_OFF);

    labelFanVal = lv_label_create(fanRow);
    lv_label_set_text(labelFanVal, "100%");
    lv_obj_set_align(labelFanVal, LV_ALIGN_RIGHT_MID);
    lv_obj_set_style_text_font(labelFanVal, &lv_font_montserrat_14, 0);

    lv_obj_add_event_cb(sliderFan, [](lv_event_t* e) {
        int val = lv_slider_get_value((lv_obj_t*)lv_event_get_target(e));
        lv_label_set_text_fmt(screenCustomEntry.labelFanVal, "%d%%", val);
    }, LV_EVENT_VALUE_CHANGED, nullptr);
}

void ScreenCustomEntry::createStep5() {
    lv_obj_t* p = stepPanels[4];

    lv_obj_t* title = lv_label_create(p);
    lv_label_set_text(title, "Review your spool settings:");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    lv_obj_t* reviewBox = lv_obj_create(p);
    lv_obj_set_size(reviewBox, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(reviewBox, lv_color_hex(0xFAFAFA), 0);
    lv_obj_set_style_radius(reviewBox, 8, 0);
    lv_obj_set_style_border_width(reviewBox, 1, 0);
    lv_obj_set_style_border_color(reviewBox, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_pad_all(reviewBox, 10, 0);
    lv_obj_clear_flag(reviewBox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(reviewBox, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(reviewBox, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(reviewBox, 2, 0);

    lblReviewBrand    = createReviewRow(reviewBox, "Brand");
    lblReviewName     = createReviewRow(reviewBox, "Name");
    lblReviewMaterial = createReviewRow(reviewBox, "Material");
    lblReviewColor    = createReviewRow(reviewBox, "Color");
    lblReviewDiameter = createReviewRow(reviewBox, "Diameter");
    lblReviewWeight   = createReviewRow(reviewBox, "Weight");
    lblReviewNozzle   = createReviewRow(reviewBox, "Nozzle Temp");
    lblReviewBed      = createReviewRow(reviewBox, "Bed Temp");
    lblReviewSpeed    = createReviewRow(reviewBox, "Speed");
    lblReviewFan      = createReviewRow(reviewBox, "Fan");
}

void ScreenCustomEntry::createColorPicker() {
    modalColor = lv_obj_create(lv_display_get_layer_top(lv_display_get_default()));
    lv_obj_set_size(modalColor, 340, 320);
    lv_obj_set_align(modalColor, LV_ALIGN_CENTER);
    lv_obj_add_flag(modalColor, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_pad_all(modalColor, 12, 0);

    lv_obj_t* title = lv_label_create(modalColor);
    lv_label_set_text(title, "Select Color");
    lv_obj_set_align(title, LV_ALIGN_TOP_MID);
    lv_obj_set_y(title, 4);

    lv_obj_t* btnClose = lv_btn_create(modalColor);
    lv_obj_set_size(btnClose, 48, 48);
    lv_obj_set_align(btnClose, LV_ALIGN_TOP_RIGHT);
    lv_obj_t* lx = lv_label_create(btnClose);
    lv_label_set_text(lx, "X");
    lv_obj_set_align(lx, LV_ALIGN_CENTER);
    lv_obj_set_style_text_font(lx, &lv_font_montserrat_20, 0);
    lv_obj_add_event_cb(btnClose, [](lv_event_t* e) {
        lv_obj_add_flag(screenCustomEntry.modalColor, LV_OBJ_FLAG_HIDDEN);
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* grid = lv_obj_create(modalColor);
    lv_obj_set_size(grid, 310, 240);
    lv_obj_set_align(grid, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_style_pad_row(grid, 6, 0);
    lv_obj_set_style_pad_column(grid, 6, 0);

    static lv_coord_t col_dsc[] = {52, 52, 52, 52, 52, LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {52, 52, 52, 52, 52, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);

    for (int i = 0; i < FILAMENT_COLOR_COUNT; i++) {
        lv_obj_t* btn = lv_btn_create(grid);
        lv_obj_set_size(btn, 52, 52);
        lv_obj_set_style_bg_color(btn, lv_color_hex(FILAMENT_COLORS[i]), 0);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_CENTER, i % 5, 1,
                             LV_GRID_ALIGN_CENTER, i / 5, 1);

        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            lv_obj_t* t = (lv_obj_t*)lv_event_get_target(e);
            lv_color_t c = lv_obj_get_style_bg_color(t, LV_PART_MAIN);
            lv_color32_t c32 = lv_color_to_32(c, LV_OPA_COVER);
            uint32_t hex = ((uint32_t)c32.red << 16) |
                           ((uint32_t)c32.green << 8) |
                           ((uint32_t)c32.blue);
            screenCustomEntry.selectedColor = hex;
            lv_obj_set_style_bg_color(screenCustomEntry.colorSwatch,
                                      lv_color_hex(hex), 0);
            lv_obj_add_flag(screenCustomEntry.modalColor, LV_OBJ_FLAG_HIDDEN);
        }, LV_EVENT_CLICKED, nullptr);
    }
}

void ScreenCustomEntry::goToStep(uint8_t step) {
    if (step >= STEP_COUNT) return;
    currentStep = step;

    // Show/hide panels
    for (int i = 0; i < STEP_COUNT; i++) {
        if (i == step)
            lv_obj_clear_flag(stepPanels[i], LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(stepPanels[i], LV_OBJ_FLAG_HIDDEN);
    }

    // Update title
    lv_label_set_text(labelStepTitle, STEP_TITLES[step]);

    // Update step indicator dots
    uint32_t dotCount = lv_obj_get_child_cnt(stepIndicator);
    for (uint32_t i = 0; i < dotCount; i++) {
        lv_obj_t* dot = lv_obj_get_child(stepIndicator, i);
        if ((int)i == step) {
            lv_obj_set_style_bg_color(dot, lv_color_hex(0x2196F3), 0);
        } else if ((int)i < step) {
            lv_obj_set_style_bg_color(dot, lv_color_hex(0x4CAF50), 0);
        } else {
            lv_obj_set_style_bg_color(dot, lv_color_hex(0xE0E0E0), 0);
        }
    }

    // Navigation visibility
    if (step == 0) {
        lv_obj_add_flag(btnPrev, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(btnPrev, LV_OBJ_FLAG_HIDDEN);
    }

    if (step == STEP_COUNT - 1) {
        lv_obj_add_flag(btnNext, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(btnSave, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(btnSaveWrite, LV_OBJ_FLAG_HIDDEN);
        populateReview();
    } else {
        lv_obj_clear_flag(btnNext, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btnSave, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btnSaveWrite, LV_OBJ_FLAG_HIDDEN);
    }
}

void ScreenCustomEntry::populateReview() {
    const char* brand = lv_textarea_get_text(taBrand);
    const char* name = lv_textarea_get_text(taName);

    char matBuf[8];
    lv_dropdown_get_selected_str(ddMaterial, matBuf, sizeof(matBuf));

    char colorBuf[8];
    snprintf(colorBuf, sizeof(colorBuf), "#%06X", selectedColor);

    uint16_t selDia = lv_dropdown_get_selected(ddDiameter);
    const char* diaStr = (selDia == 0) ? "1.75 mm" : "2.85 mm";

    char buf[32];
    snprintf(buf, sizeof(buf), "%d g", (int)lv_spinbox_get_value(spinWeight));
    lv_label_set_text(lblReviewWeight, buf);

    snprintf(buf, sizeof(buf), "%d - %d C",
             (int)lv_spinbox_get_value(spinNozzleMin),
             (int)lv_spinbox_get_value(spinNozzleMax));
    lv_label_set_text(lblReviewNozzle, buf);

    snprintf(buf, sizeof(buf), "%d - %d C",
             (int)lv_spinbox_get_value(spinBedMin),
             (int)lv_spinbox_get_value(spinBedMax));
    lv_label_set_text(lblReviewBed, buf);

    snprintf(buf, sizeof(buf), "%d - %d mm/s",
             (int)lv_spinbox_get_value(spinSpeedMin),
             (int)lv_spinbox_get_value(spinSpeedMax));
    lv_label_set_text(lblReviewSpeed, buf);

    snprintf(buf, sizeof(buf), "%d%%", (int)lv_slider_get_value(sliderFan));
    lv_label_set_text(lblReviewFan, buf);

    lv_label_set_text(lblReviewBrand, (brand && brand[0]) ? brand : "(none)");
    lv_label_set_text(lblReviewName, (name && name[0]) ? name : "(unnamed)");
    lv_label_set_text(lblReviewMaterial, matBuf);
    lv_label_set_text(lblReviewColor, colorBuf);
    lv_label_set_text(lblReviewDiameter, diaStr);
}

void ScreenCustomEntry::reset() {
    _lazyInit();
    lv_textarea_set_text(taBrand, "");
    lv_textarea_set_text(taName, "");
    lv_dropdown_set_selected(ddMaterial, 0);
    selectedColor = 0xFFFFFF;
    lv_obj_set_style_bg_color(colorSwatch, lv_color_hex(0xFFFFFF), 0);
    lv_dropdown_set_selected(ddDiameter, 0);
    lv_spinbox_set_value(spinWeight, 1000);
    lv_spinbox_set_value(spinNozzleMin, 190);
    lv_spinbox_set_value(spinNozzleMax, 220);
    lv_spinbox_set_value(spinBedMin, 50);
    lv_spinbox_set_value(spinBedMax, 70);
    lv_spinbox_set_value(spinSpeedMin, 40);
    lv_spinbox_set_value(spinSpeedMax, 100);
    lv_slider_set_value(sliderFan, 100, LV_ANIM_OFF);
    lv_label_set_text(labelFanVal, "100%");
    goToStep(0);
}

void ScreenCustomEntry::show() {
    _lazyInit();
    lv_screen_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}
