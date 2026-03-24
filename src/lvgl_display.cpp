#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include "lvgl_display.h"
#include "LGFX_Config.h"

/* MaTouch has no CH422G expander — touch RST is GPIO38 (handled by LGFX Touch_GT911 cfg),
 * backlight is GPIO2 PWM (handled by LGFX Light_PWM cfg). No expander init needed. */

LV_IMG_DECLARE(logo_thingy);

static LGFX gfx;
static lv_obj_t* splash_screen;
static lv_obj_t* status_container;
static lv_obj_t* continue_btn = nullptr;
static bool      continue_pressed = false;
static int status_label_count = 0;

static void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    uint16_t *pixels = (uint16_t *)px_map;
    gfx.startWrite();
    gfx.setAddrWindow(area->x1, area->y1, w, h);
    gfx.writePixels(pixels, w * h, true);
    gfx.endWrite();
    lv_display_flush_ready(disp);
}

void my_touch_read(lv_indev_t *indev, lv_indev_data_t *data) {
    uint16_t touchX, touchY;
    bool touched = gfx.getTouch(&touchX, &touchY);
    if (touched) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = (int16_t)touchX;
        data->point.y = (int16_t)touchY;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void lvgl_display_init() {
    gfx.begin();
    gfx.fillScreen(TFT_BLACK);
    lv_init();

    // Wire LVGL's tick to Arduino millis() so the display refresh timer
    // fires correctly during setup() — not just in loop().  Without this,
    // lv_tick_get() returns 0 throughout setup(), the 30 ms refr period
    // never elapses, dirty areas never flush, and the splash status labels
    // are never rendered to the physical display until loop() starts.
    lv_tick_set_cb([]() -> uint32_t { return (uint32_t)millis(); });

    const uint32_t buf_pixels = (gfx.width() * gfx.height()) / 12;
    const uint32_t buf_size_bytes = buf_pixels * sizeof(lv_color_t);
    uint8_t *buf1 = (uint8_t *)heap_caps_malloc(buf_size_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    LV_ASSERT_MALLOC(buf1);

    lv_display_t *disp = lv_display_create(gfx.width(), gfx.height());
    lv_display_set_buffers(disp, buf1, NULL, buf_size_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_default(disp);

    lv_indev_t *touch_indev = lv_indev_create();
    lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch_indev, my_touch_read);
    lv_indev_set_display(touch_indev, disp);

    splash_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(splash_screen, lv_color_white(), 0);
    lv_screen_load(splash_screen);

    lv_obj_t * splash_img = lv_img_create(splash_screen);
    lv_img_set_src(splash_img, &logo_thingy);
    lv_obj_align(splash_img, LV_ALIGN_CENTER, 0, -60);
    lv_img_set_zoom(splash_img, 512);

    status_container = lv_obj_create(splash_screen);
    lv_obj_set_size(status_container, 700, 160);
    lv_obj_align(status_container, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_opa(status_container, 0, 0);
    lv_obj_set_style_border_width(status_container, 0, 0);
    lv_obj_set_flex_flow(status_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(status_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(status_container, 5, 0);

    lv_timer_handler();
}

void splash_add_status(const char* text, bool initial_state) {
    lv_obj_t* label = lv_label_create(status_container);
    lv_label_set_text_fmt(label, "%s: ...", text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(label, lv_color_make(128, 128, 128), 0);
    status_label_count++;
    lv_timer_handler();
}

void splash_update_status(int index, const char* status_text, bool success) {
    if (index < (int)lv_obj_get_child_cnt(status_container)) {
        lv_obj_t* label = lv_obj_get_child(status_container, index);
        const char* titles[] = {"WiFi", "RFID", "Bluetooth", "Filament DB"};
        const char* title = (index < 4) ? titles[index] : "Status";
        lv_label_set_text_fmt(label, "%s: %s", title, status_text);
        lv_obj_set_style_text_color(label, success ? lv_color_make(0, 150, 0) : lv_color_make(200, 0, 0), 0);
        lv_timer_handler();
    }
}

void lvgl_display_start_ui() {}

bool splash_is_continue_pressed() { return continue_pressed; }

void splash_show_continue_button() {
    if (continue_btn) return;  // already created

    continue_btn = lv_btn_create(splash_screen);
    lv_obj_set_size(continue_btn, 80, 50);
    lv_obj_align(continue_btn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

    lv_obj_t* lbl = lv_label_create(continue_btn);
    lv_label_set_text(lbl, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_align(lbl, LV_ALIGN_CENTER);

    lv_obj_add_event_cb(continue_btn, [](lv_event_t*) {
        continue_pressed = true;
    }, LV_EVENT_CLICKED, NULL);

    lv_timer_handler();
}
