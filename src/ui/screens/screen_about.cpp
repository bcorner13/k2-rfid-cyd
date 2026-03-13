/**
 * @file screen_about.cpp
 * @brief About screen — hardware specs and firmware version info.
 *
 * Displays board identity, display/touch/RFID specs, chip info, and
 * LittleFS storage usage. Content is populated at show() time so that
 * runtime values (heap, LittleFS) are always current.
 */

#include "ui/screens/screen_about.h"
#include <Arduino.h>
#include "esp_chip_info.h"
#include <LittleFS.h>
#include "board_pins.h"

ScreenAbout screenAbout;

// ─── Firmware version string ─────────────────────────────────────────────────
// Bump this whenever a release is cut.
#define FW_VERSION  "0.1.0-dev"

static void back_event_cb(lv_event_t* e) {
    // uiSound.playClick();  // Uncomment when uiSound.init() is called in main
    // lv_scr_load(screenMain.screen);
}

void ScreenAbout::init() {
    screen = lv_obj_create(NULL);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

    // --- Title ---
    labelTitle = lv_label_create(screen);
    lv_label_set_text(labelTitle, "About This Device");
    lv_obj_set_style_text_font(labelTitle, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(labelTitle, lv_color_white(), 0);
    lv_obj_set_align(labelTitle, LV_ALIGN_TOP_MID);
    lv_obj_set_y(labelTitle, 18);

    // --- Back Button ---
    btnBack = lv_btn_create(screen);
    lv_obj_set_size(btnBack, 60, 44);
    lv_obj_set_align(btnBack, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(btnBack, 16, 12);
    lv_obj_t* lBack = lv_label_create(btnBack);
    lv_label_set_text(lBack, LV_SYMBOL_LEFT);
    lv_obj_set_align(lBack, LV_ALIGN_CENTER);
    lv_obj_add_event_cb(btnBack, back_event_cb, LV_EVENT_CLICKED, NULL);

    // --- Two-column info container ---
    lv_obj_t* infoContainer = lv_obj_create(screen);
    lv_obj_set_size(infoContainer, 760, 390);
    lv_obj_set_align(infoContainer, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_y(infoContainer, -8);
    lv_obj_set_style_bg_color(infoContainer, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_border_color(infoContainer, lv_color_hex(0x3A3A5C), 0);
    lv_obj_set_style_border_width(infoContainer, 1, 0);
    lv_obj_set_style_radius(infoContainer, 8, 0);
    lv_obj_set_style_pad_all(infoContainer, 14, 0);
    lv_obj_set_style_pad_row(infoContainer, 6, 0);
    lv_obj_set_flex_flow(infoContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(infoContainer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Helper: create a styled info label
    auto makeLabel = [&](lv_obj_t* parent) -> lv_obj_t* {
        lv_obj_t* lbl = lv_label_create(parent);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xCCCCDD), 0);
        lv_obj_set_width(lbl, LV_PCT(100));
        return lbl;
    };

    labelBoard    = makeLabel(infoContainer);
    labelDisplay  = makeLabel(infoContainer);
    labelRFID     = makeLabel(infoContainer);
    labelFWInfo   = makeLabel(infoContainer);
    labelChipInfo = makeLabel(infoContainer);
    labelMemInfo  = makeLabel(infoContainer);
    labelFlashInfo = makeLabel(infoContainer);
    labelStorageInfo = makeLabel(infoContainer);

}

void ScreenAbout::show() {
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    uint32_t flash_mb  = ESP.getFlashChipSize()  / (1024 * 1024);
    uint32_t psram_mb  = ESP.getPsramSize()       / (1024 * 1024);
    uint32_t heap_kb   = ESP.getFreeHeap()        / 1024;

    // --- Board identity ---
#if defined(BOARD_MATOUCH_V31)
    lv_label_set_text(labelBoard, LV_SYMBOL_SETTINGS "  MaTouch ESP32-S3 4.3\" V3.1");
#elif defined(BOARD_MATOUCH_ESP32_S3_43)
    lv_label_set_text(labelBoard, LV_SYMBOL_SETTINGS "  MaTouch ESP32-S3 4.3\" (MaTouch)");
#else
    lv_label_set_text(labelBoard, LV_SYMBOL_SETTINGS "  MaTouch ESP32-S3 4.3\"");
#endif

    // --- Display & Touch ---
    lv_label_set_text(labelDisplay,
        LV_SYMBOL_IMAGE "  800\xc3\x97" "480 IPS RGB565  |  GT911 capacitive touch (5-pt)");

    // --- RFID ---
    lv_label_set_text(labelRFID,
        LV_SYMBOL_WIFI "  PN532 RFID/NFC  |  I2C 0x24  |  Mabee I2C port (GPIO17/18)");

    // --- Firmware ---
    lv_label_set_text_fmt(labelFWInfo,
        LV_SYMBOL_FILE "  FW v" FW_VERSION "  |  ESP-IDF %s  |  LVGL %d.%d.%d",
        esp_get_idf_version(),
        LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);

    // --- Chip ---
    lv_label_set_text_fmt(labelChipInfo,
        LV_SYMBOL_CHARGE "  ESP32-S3  rev %d  |  %d cores  |  WiFi + BT 5.0",
        chip_info.revision, chip_info.cores);

    // --- Memory ---
    lv_label_set_text_fmt(labelMemInfo,
        LV_SYMBOL_REFRESH "  PSRAM %luMB OPI  |  Heap free %luKB",
        (unsigned long)psram_mb, (unsigned long)heap_kb);

    // --- Flash ---
    lv_label_set_text_fmt(labelFlashInfo,
        LV_SYMBOL_SAVE "  Flash %luMB  |  32GB MicroSD",
        (unsigned long)flash_mb);

    // --- LittleFS storage ---
    if (LittleFS.begin(false)) {
        lv_label_set_text_fmt(labelStorageInfo,
            LV_SYMBOL_DIRECTORY "  LittleFS: %u / %u KB used",
            (unsigned int)(LittleFS.usedBytes()  / 1024),
            (unsigned int)(LittleFS.totalBytes() / 1024));
    } else {
        lv_label_set_text(labelStorageInfo,
            LV_SYMBOL_DIRECTORY "  LittleFS: not mounted");
    }

    lv_screen_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}
