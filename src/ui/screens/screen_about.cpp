#include <ui/screens/screen_about.h> // Updated include path
// #include "ui/ui_sound.h" // Commented out as sound is tabled
#include <Arduino.h>
#include "esp_chip_info.h"
#include <LittleFS.h> // Changed from SPIFFS.h, now using angle brackets

ScreenAbout screenAbout;

static void back_event_cb(lv_event_t* e) {
    // uiSound.playClick(); // Commented out as sound is tabled
    // Placeholder: Go back to main menu
    // lv_scr_load(screenMain.screen); // This will be updated later when screenMain is available
}

void ScreenAbout::init() {
    screen = lv_obj_create(NULL);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

    // --- Title ---
    labelTitle = lv_label_create(screen);
    lv_label_set_text(labelTitle, "About This Device");
    lv_obj_set_style_text_font(labelTitle, &lv_font_montserrat_24, 0);
    lv_obj_set_align(labelTitle, LV_ALIGN_TOP_MID); // Changed from lv_obj_align
    lv_obj_set_y(labelTitle, 20); // Set y offset

    // --- Back Button ---
    btnBack = lv_btn_create(screen);
    lv_obj_set_size(btnBack, 60, 60);
    lv_obj_set_align(btnBack, LV_ALIGN_TOP_LEFT); // Changed from lv_obj_align
    lv_obj_set_x(btnBack, 20); // Set x offset
    lv_obj_set_y(btnBack, 20); // Set y offset
    lv_obj_t* lBack = lv_label_create(btnBack);
    lv_label_set_text(lBack, LV_SYMBOL_LEFT);
    lv_obj_set_align(lBack, LV_ALIGN_CENTER); // Changed from lv_obj_center
    lv_obj_add_event_cb(btnBack, back_event_cb, LV_EVENT_CLICKED, NULL);

    // --- Info Container ---
    lv_obj_t* infoContainer = lv_obj_create(screen);
    lv_obj_set_size(infoContainer, 760, 380);
    lv_obj_set_align(infoContainer, LV_ALIGN_BOTTOM_MID); // Changed from lv_obj_align
    lv_obj_set_y(infoContainer, -10); // Set y offset
    lv_obj_set_flex_flow(infoContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(infoContainer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(infoContainer, 10, 0);

    labelBoard = lv_label_create(infoContainer);
    labelESPVersion = lv_label_create(infoContainer);
    labelLVGLVersion = lv_label_create(infoContainer);
    labelChipInfo = lv_label_create(infoContainer);
    labelMemInfo = lv_label_create(infoContainer);
    labelFlashInfo = lv_label_create(infoContainer);
    labelStorageInfo = lv_label_create(infoContainer);

    lv_obj_set_style_text_font(labelBoard, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_font(labelESPVersion, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_font(labelLVGLVersion, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_font(labelChipInfo, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_font(labelMemInfo, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_font(labelFlashInfo, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_font(labelStorageInfo, &lv_font_montserrat_20, 0);
}

void ScreenAbout::show() {
    // --- Gather Info ---
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    uint32_t flash_size_mb = ESP.getFlashChipSize() / (1024 * 1024);

    // --- Populate Labels ---
#ifdef BOARD_ESP32_S3_TOUCH_LCD_4_3C
    lv_label_set_text(labelBoard, "Board: ESP32-S3-Touch-LCD-4.3C");
#else
    lv_label_set_text(labelBoard, "Board: (see build)");
#endif
    lv_label_set_text_fmt(labelESPVersion, "ESP-IDF: %s", esp_get_idf_version());
    lv_label_set_text_fmt(labelLVGLVersion, "LVGL: %d.%d.%d", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
    lv_label_set_text_fmt(labelChipInfo, "Chip: %s rev %d, %d cores", "ESP32-S3", chip_info.revision, chip_info.cores);
    lv_label_set_text_fmt(labelMemInfo, "Memory: %dMB PSRAM, %dKB SRAM", ESP.getPsramSize() / 1024 / 1024, ESP.getHeapSize() / 1024);
    lv_label_set_text_fmt(labelFlashInfo, "Flash: %dMB", flash_size_mb);

    if (LittleFS.begin(false)) { // Changed from SPIFFS.begin
        lv_label_set_text_fmt(labelStorageInfo, "Storage: %u / %u KB used", (unsigned int)(LittleFS.usedBytes() / 1024), (unsigned int)(LittleFS.totalBytes() / 1024)); // Changed from SPIFFS
    } else {
        lv_label_set_text(labelStorageInfo, "Storage: Not Mounted");
    }

    lv_screen_load(screen); // Changed from lv_scr_load
}