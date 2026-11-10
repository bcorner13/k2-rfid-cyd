#include <ui/screens/screen_spool_detail.h>
#include <ui/screens/screen_inventory.h>
#include <cstdio>

ScreenSpoolDetail screenSpoolDetail;

void ScreenSpoolDetail::init() {}   // created on first show, like before (boot-time LVGL load)

void ScreenSpoolDetail::lazyInit() {
    if (screen) return;
    screen = lv_obj_create(nullptr);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);

    btnBack = lv_btn_create(screen);
    lv_obj_set_size(btnBack, 70, 36);
    lv_obj_set_pos(btnBack, 10, 7);
    lv_obj_t* lblBack = lv_label_create(btnBack);
    lv_label_set_text(lblBack, LV_SYMBOL_LEFT);
    lv_obj_set_align(lblBack, LV_ALIGN_CENTER);

    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "Spool");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    labelStatus = lv_label_create(screen);
    lv_label_set_text(labelStatus, "");
    lv_obj_set_style_text_font(labelStatus, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(labelStatus, lv_color_hex(0x555555), 0);
    lv_obj_align(labelStatus, LV_ALIGN_TOP_RIGHT, -12, 16);

    header = lv_obj_create(screen);
    lv_obj_set_size(header, 776, 90);
    lv_obj_set_pos(header, 12, 56);
    lv_obj_set_style_bg_color(header, lv_color_hex(0xF5F5F5), 0);
    lv_obj_set_style_radius(header, 8, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 12, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    labelName = lv_label_create(header);
    lv_label_set_long_mode(labelName, LV_LABEL_LONG_DOT);
    lv_obj_set_width(labelName, 660);
    lv_obj_set_pos(labelName, 84, 18);
    lv_obj_set_style_text_font(labelName, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(labelName, lv_color_black(), 0);

    labelInfo = lv_label_create(screen);
    lv_obj_set_pos(labelInfo, 24, 164);
    lv_obj_set_width(labelInfo, 752);
    lv_obj_set_style_text_font(labelInfo, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(labelInfo, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_line_space(labelInfo, 8, 0);

    btnWriteTag = lv_btn_create(screen);
    lv_obj_set_size(btnWriteTag, 360, 64);
    lv_obj_align(btnWriteTag, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(btnWriteTag, lv_color_hex(0x2196F3), 0);
    lv_obj_t* lblWrite = lv_label_create(btnWriteTag);
    lv_label_set_text(lblWrite, LV_SYMBOL_EDIT "  Write tag for this spool");
    lv_obj_set_style_text_font(lblWrite, &lv_font_montserrat_18, 0);
    lv_obj_set_align(lblWrite, LV_ALIGN_CENTER);
}

void ScreenSpoolDetail::loadSpool(const SpoolRow& r, const char* statusText) {
    lazyInit();
    currentSpoolId = r.id;

    if (swatch) lv_obj_delete(swatch);
    swatch = makeSpoolSwatch(header, r, 66);
    lv_obj_set_align(swatch, LV_ALIGN_LEFT_MID);

    char buf[320];
    if (!r.vendor.empty()) snprintf(buf, sizeof(buf), "%s %s", r.vendor.c_str(), r.name.c_str());
    else snprintf(buf, sizeof(buf), "%s", r.name.empty() ? "(unknown filament)" : r.name.c_str());
    lv_label_set_text(labelName, buf);
    lv_label_set_text(labelStatus, statusText);

    char idText[48];
    if (r.id < 0) snprintf(idText, sizeof(idText), "new (not in Spoolman yet)");
    else snprintf(idText, sizeof(idText), "#%d", r.id);
    char remaining[24];
    if (r.hasRemaining) snprintf(remaining, sizeof(remaining), "%.0f g", r.remainingG);
    else snprintf(remaining, sizeof(remaining), "unknown");
    snprintf(buf, sizeof(buf),
             "Spoolman spool:  %s%s\n"
             "Material:  %s%s%s\n"
             "Spool size:  %.0f g      Remaining:  %s\n"
             "Location:  %s\n"
             "Tag:  %s%s",
             idText, r.archived ? "  (archived)" : "",
             r.material.empty() ? "-" : r.material.c_str(), r.cfsId.empty() ? "" : "   CFS id ",
             r.cfsId.c_str(), r.initialG, remaining, r.location.empty() ? "-" : r.location.c_str(),
             r.uid.empty() ? "no tag" : r.uid.c_str(), r.pending ? "\n\n" LV_SYMBOL_REFRESH "  Not synced to Spoolman yet" : "");
    lv_label_set_text(labelInfo, buf);
}

void ScreenSpoolDetail::show() {
    lazyInit();
    lv_screen_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}
