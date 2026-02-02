#include <ui/screens/screen_settings.h> // Updated include path
#include <config_manager.h> // Updated include path

void ScreenSettings::init() {
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);

    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_align(title, LV_ALIGN_TOP_MID); // Changed from lv_obj_align
    lv_obj_set_y(title, 20); // Set y offset

    btnBack = lv_btn_create(screen);
    lv_obj_set_size(btnBack, 60, 60);
    lv_obj_set_align(btnBack, LV_ALIGN_TOP_LEFT); // Changed from lv_obj_align
    lv_obj_set_x(btnBack, 20); // Set x offset
    lv_obj_set_y(btnBack, 20); // Set y offset
    lv_obj_t* lblBack = lv_label_create(btnBack);
    lv_label_set_text(lblBack, LV_SYMBOL_LEFT);
    lv_obj_set_align(lblBack, LV_ALIGN_CENTER); // Changed from lv_obj_center

    lv_obj_t* cont = lv_obj_create(screen);
    lv_obj_set_size(cont, 400, 380);
    lv_obj_set_align(cont, LV_ALIGN_BOTTOM_MID); // Changed from lv_obj_align
    lv_obj_set_y(cont, -10); // Set y offset
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Beep
    lv_obj_t* p1 = lv_obj_create(cont);
    lv_obj_set_size(p1, LV_PCT(100), 50);
    lv_obj_set_style_bg_opa(p1, 0, 0);
    lv_obj_set_style_border_width(p1, 0, 0);
    lv_obj_t* l1 = lv_label_create(p1);
    lv_label_set_text(l1, "Beep on R/W");
    lv_obj_set_align(l1, LV_ALIGN_LEFT_MID); // Changed from lv_obj_align
    lv_obj_set_x(l1, 0); // Set x offset
    lv_obj_set_style_text_font(l1, &lv_font_montserrat_20, 0);
    swBeep = lv_switch_create(p1);
    lv_obj_set_align(swBeep, LV_ALIGN_RIGHT_MID); // Changed from lv_obj_align
    lv_obj_set_x(swBeep, 0); // Set x offset
    if(config.data.beep_enabled) lv_obj_add_state(swBeep, LV_STATE_CHECKED);

    // Update DB Button
    btnUpdateDB = lv_btn_create(cont);
    lv_obj_set_size(btnUpdateDB, 300, 50);
    lv_obj_t* lUp = lv_label_create(btnUpdateDB);
    lv_label_set_text(lUp, "Update Database");
    lv_obj_set_align(lUp, LV_ALIGN_CENTER); // Changed from lv_obj_center

    // Reset WiFi Button
    btnResetWifi = lv_btn_create(cont);
    lv_obj_set_size(btnResetWifi, 300, 50);
    lv_obj_t* lWifi = lv_label_create(btnResetWifi);
    lv_label_set_text(lWifi, "Reset WiFi");
    lv_obj_set_align(lWifi, LV_ALIGN_CENTER); // Changed from lv_obj_center

    // About Button
    btnAbout = lv_btn_create(cont);
    lv_obj_set_size(btnAbout, 300, 50);
    lv_obj_t* lAbout = lv_label_create(btnAbout);
    lv_label_set_text(lAbout, "About");
    lv_obj_set_align(lAbout, LV_ALIGN_CENTER); // Changed from lv_obj_center

    // Restart Button
    btnRestart = lv_btn_create(cont);
    lv_obj_set_size(btnRestart, 300, 50);
    lv_obj_t* l4 = lv_label_create(btnRestart);
    lv_label_set_text(l4, "Restart Device");
    lv_obj_set_align(l4, LV_ALIGN_CENTER); // Changed from lv_obj_center
}

void ScreenSettings::show() {
    lv_screen_load(screen); // Changed from lv_scr_load
}