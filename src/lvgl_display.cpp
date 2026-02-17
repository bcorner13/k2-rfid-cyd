#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include "lvgl_display.h"
#include "LGFX_Config.h"

#if BOARD_ESP32_S3_TOUCH_LCD_4_3C
#include <esp_io_expander.hpp>
#endif

LV_IMG_DECLARE(logo_thingy);

static LGFX gfx;

#if BOARD_ESP32_S3_TOUCH_LCD_4_3C
/* CH422G (U10) on I2C GPIO8/9: EXIO1=TP_RST, EXIO2=DISP. Init before display so touch and backlight work like official demos. */
static void init_ch422g_4_3c()
{
    const int SDA = 8, SCL = 9;
    esp_expander::CH422G expander(SCL, SDA, ESP_IO_EXPANDER_I2C_CH422G_ADDRESS);
    if (!expander.init() || !expander.begin()) {
        Serial.println("CH422G init/begin failed (touch reset and backlight may not work)");
        return;
    }
    /* begin() sets IO0-7 output high → EXIO1 (TP_RST) and EXIO2 (DISP) high. Run touch reset sequence (official demo style). */
    const int EXIO1_TP_RST = 1;
    expander.digitalWrite(EXIO1_TP_RST, LOW);
    delay(20);
    expander.digitalWrite(EXIO1_TP_RST, HIGH);
    delay(50);   /* let GT911 come out of reset before LGFX/touch init */
    Serial.println("CH422G OK (TP_RST pulse, DISP high)");
}
#endif
static lv_obj_t* splash_screen;
static lv_obj_t* status_container;
static int status_label_count = 0;



static void my_disp_flush(
    lv_display_t *disp,
    const lv_area_t *area,
    uint8_t *px_map
)
{
    Serial.printf(
        "FLUSH x:%d y:%d w:%d h:%d\n",
        area->x1, area->y1,
        area->x2 - area->x1 + 1,
        area->y2 - area->y1 + 1
    );
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    uint16_t *pixels = (uint16_t *)px_map;

    gfx.startWrite();
    gfx.setAddrWindow(area->x1, area->y1, w, h);
    gfx.writePixels(pixels, w * h, true);
    gfx.endWrite();

    lv_display_flush_ready(disp);
}


void my_touch_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    uint16_t touchX, touchY;
    bool touched = gfx.getTouch(&touchX, &touchY);
    if (touched) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = (int16_t)touchX;
        data->point.y = (int16_t)touchY;
    } else {
        data->state = LV_INDEV_STATE_REL;
        /* Keep previous point coordinates — LVGL needs the last valid
           position for release events; reporting (0,0) causes sliders
           and other drag widgets to jump to the origin on release. */
    }
}

void lvgl_display_init()
{
#if BOARD_ESP32_S3_TOUCH_LCD_4_3C
    init_ch422g_4_3c();
#endif
    gfx.begin();
    gfx.fillScreen(TFT_RED);
    delay(500);
    gfx.fillScreen(TFT_GREEN);
    delay(500);
    gfx.fillScreen(TFT_BLUE);
    delay(500);
    /* 4.3C backlight is via U10 (I2C), not LGFX setBrightness – enable when U10 driver is added */
    lv_init();

    /* ---------------- Display buffer (LVGL 9: lv_display_set_buffers) ---------------- */
    const uint32_t buf_pixels = (gfx.width() * gfx.height()) / 10;
    const uint32_t buf_size_bytes = buf_pixels * sizeof(lv_color_t);

    uint8_t *buf1 = (uint8_t *)heap_caps_malloc(buf_size_bytes, MALLOC_CAP_SPIRAM);
    LV_ASSERT_MALLOC(buf1);

    Serial.print("Initialized Display Buffer");

    /* ---------------- Display object ---------------- */
    lv_display_t *disp = lv_display_create(gfx.width(), gfx.height());

    lv_display_set_buffers(
        disp,
        buf1,
        NULL,
        buf_size_bytes,
        LV_DISPLAY_RENDER_MODE_PARTIAL
    );

    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_default(disp);
    Serial.printf("disp=%p default=%p\n",
                  disp,
                  lv_display_get_default());
    /* ---------------- Touch input ---------------- */
    lv_indev_t *touch_indev = lv_indev_create();
    lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch_indev, my_touch_read);
    lv_indev_set_display(touch_indev, disp);  /* LVGL 9: bind input to display */

    /* ---------------- Splash screen ---------------- */
    splash_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(
        splash_screen,
        lv_color_white(),
        LV_PART_MAIN | LV_STATE_DEFAULT
    );

    lv_screen_load(splash_screen);

    lv_obj_t *splash_img = lv_img_create(splash_screen);
    lv_img_set_src(splash_img, &logo_thingy);
    lv_obj_align(splash_img, LV_ALIGN_CENTER, 0, -30);
    lv_img_set_zoom(splash_img, 512);   // ~2x

    status_container = lv_obj_create(splash_screen);
    lv_obj_set_size(status_container, 700, 160);
    lv_obj_align(status_container, LV_ALIGN_CENTER, 0, 80);

    lv_obj_set_style_bg_opa(
        status_container,
        LV_OPA_TRANSP,
        LV_PART_MAIN | LV_STATE_DEFAULT
    );
    lv_obj_set_style_border_width(
        status_container,
        0,
        LV_PART_MAIN | LV_STATE_DEFAULT
    );

    lv_obj_set_flex_flow(status_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(
        status_container,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    /* ---------------- Force initial render ---------------- */
    for (int i = 0; i < 10; i++) {
        lv_tick_inc(10);
        lv_timer_handler();
        delay(10);
    }
    Serial.println("Priming LVGL timebase");

    lv_tick_inc(1);                 // create time
    lv_timer_handler();             // process invalidations
    lv_refr_now(lv_display_get_default()); // force first flush

    Serial.println("LVGL primed");
}


void splash_add_status(const char* text, bool initial_state) {
    lv_obj_t* label = lv_label_create(status_container);
    lv_label_set_text_fmt(label, "%s: ...", text);
    // lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0); // Use default font to ensure visibility
    lv_obj_set_style_text_color(label, lv_color_black(), 0);
    status_label_count++;
    // Force render so user sees "Connecting..." immediately
    lv_timer_handler();
    delay(10);
}

void splash_update_status(int index, const char* status_text, bool success) {
    if (index < lv_obj_get_child_cnt(status_container)) {
        lv_obj_t* label = lv_obj_get_child(status_container, index);
        const char* base_text = lv_label_get_text(label);
        // Extract the base text before the colon
        char base_text_trunc[64];
        int i = 0;
        while(base_text[i] != ':' && base_text[i] != '\0' && i < 63) {
            base_text_trunc[i] = base_text[i];
            i++;
        }
        base_text_trunc[i] = '\0';

        lv_label_set_text_fmt(label, "%s: %s", base_text_trunc, status_text);
        // Replaced lv_palette_main with lv_color_make
        lv_obj_set_style_text_color(label, success ? lv_color_make(0, 128, 0) : lv_color_make(255, 0, 0), 0);

        // Refresh the screen to show the update
        lv_timer_handler();
        delay(50);
    }
}

void lvgl_display_start_ui() {
    // Touch driver moved to lvgl_display_init
}