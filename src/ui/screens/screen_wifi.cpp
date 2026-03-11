#include <ui/screens/screen_wifi.h>

ScreenWifi screenWifi;

void ScreenWifi::init() {
    screen = lv_obj_create(NULL);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);

    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "WiFi Setup");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_align(title, LV_ALIGN_TOP_MID);
    lv_obj_set_y(title, 20);

    btnBack = lv_btn_create(screen);
    lv_obj_set_size(btnBack, 60, 60);
    lv_obj_set_align(btnBack, LV_ALIGN_TOP_LEFT);
    lv_obj_set_x(btnBack, 20);
    lv_obj_set_y(btnBack, 20);
    lv_obj_t* lblBack = lv_label_create(btnBack);
    lv_label_set_text(lblBack, LV_SYMBOL_LEFT);
    lv_obj_set_align(lblBack, LV_ALIGN_CENTER);

    lv_obj_t* cont = lv_obj_create(screen);
    lv_obj_set_size(cont, 600, 300);
    lv_obj_set_align(cont, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(cont, 0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* l1 = lv_label_create(cont);
    lv_label_set_text(l1, "Connect to WiFi AP:");
    lv_obj_set_style_text_font(l1, &lv_font_montserrat_20, 0);

    lv_obj_t* l2 = lv_label_create(cont);
    lv_label_set_text(l2, "K2-RFID-SETUP");
    lv_obj_set_style_text_font(l2, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(l2, lv_color_make(0x21, 0x96, 0xF3), 0);

    lv_obj_t* l3 = lv_label_create(cont);
    lv_label_set_text(l3, "Then browse to 192.168.4.1");
    lv_obj_set_style_text_font(l3, &lv_font_montserrat_18, 0);

    labelStatus = lv_label_create(cont);
    lv_label_set_text(labelStatus, "Portal Active");
    lv_obj_set_style_text_font(labelStatus, &lv_font_montserrat_14, 0);
    lv_obj_set_y(labelStatus, 20);
}

void ScreenWifi::show() {
    lv_screen_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}

void ScreenWifi::setStatus(const char* text) {
    if(labelStatus) {
        lv_label_set_text(labelStatus, text);
    }
}
