#include <ui/screens/screen_inventory.h>
#include <cstdio>
#include <cstdlib>

ScreenInventory screenInventory;

namespace {
constexpr uint32_t PENDING_COLOR = 0xB26A00;   // amber, as the main screen's "Not in Spoolman"

uint32_t hexColor(const std::string& hex, uint32_t fallback = 0xCCCCCC) {
    return hex.size() == 6 ? (uint32_t)strtoul(hex.c_str(), nullptr, 16) : fallback;
}

void noClick(lv_obj_t* o) {
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}
}  // namespace

lv_obj_t* makeSpoolSwatch(lv_obj_t* parent, const SpoolRow& r, int32_t size) {
    lv_obj_t* sw = lv_obj_create(parent);
    lv_obj_set_size(sw, size, size);
    lv_obj_set_style_radius(sw, 6, 0);
    lv_obj_set_style_border_width(sw, 1, 0);
    lv_obj_set_style_border_color(sw, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_pad_all(sw, 0, 0);
    lv_obj_set_style_clip_corner(sw, true, 0);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
    noClick(sw);

    if (r.multiColors.size() >= 2) {
        lv_obj_set_layout(sw, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(sw, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(sw, 0, 0);
        for (const auto& c : r.multiColors) {
            lv_obj_t* stripe = lv_obj_create(sw);
            lv_obj_set_height(stripe, LV_PCT(100));
            lv_obj_set_flex_grow(stripe, 1);
            lv_obj_set_style_radius(stripe, 0, 0);
            lv_obj_set_style_border_width(stripe, 0, 0);
            lv_obj_set_style_bg_color(stripe, lv_color_hex(hexColor(c)), 0);
            lv_obj_set_style_bg_opa(stripe, LV_OPA_COVER, 0);
            noClick(stripe);
        }
    } else {
        lv_obj_set_style_bg_color(sw, lv_color_hex(hexColor(r.colorHex)), 0);
    }
    return sw;
}

void ScreenInventory::init() {
    screen = lv_obj_create(nullptr);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);

    lv_obj_t* titleBar = lv_obj_create(screen);
    lv_obj_set_size(titleBar, LV_PCT(100), 50);
    lv_obj_set_align(titleBar, LV_ALIGN_TOP_MID);
    lv_obj_set_style_bg_opa(titleBar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(titleBar, 0, 0);
    lv_obj_set_style_pad_all(titleBar, 0, 0);
    lv_obj_clear_flag(titleBar, LV_OBJ_FLAG_SCROLLABLE);

    btnBack = lv_btn_create(titleBar);
    lv_obj_set_size(btnBack, 70, 36);
    lv_obj_set_align(btnBack, LV_ALIGN_LEFT_MID);
    lv_obj_set_x(btnBack, 10);
    lv_obj_t* lblBack = lv_label_create(btnBack);
    lv_label_set_text(lblBack, LV_SYMBOL_LEFT);
    lv_obj_set_align(lblBack, LV_ALIGN_CENTER);

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

    labelStatus = lv_label_create(screen);
    lv_label_set_text(labelStatus, "");
    lv_obj_set_style_text_font(labelStatus, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(labelStatus, lv_color_hex(0x555555), 0);
    lv_obj_align(labelStatus, LV_ALIGN_TOP_MID, 0, 56);

    list = lv_obj_create(screen);
    lv_obj_set_size(list, LV_PCT(96), 390);
    lv_obj_set_align(list, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_y(list, -8);
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

void ScreenInventory::addSpoolRow(const SpoolRow& r) {
    lv_obj_t* row = lv_obj_create(list);
    lv_obj_set_size(row, LV_PCT(100), 64);
    lv_obj_set_style_bg_color(row, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_pad_all(row, 8, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(row, (void*)(intptr_t)r.id);

    lv_obj_t* sw = makeSpoolSwatch(row, r, 40);
    lv_obj_set_align(sw, LV_ALIGN_LEFT_MID);

    char buf[96];
    lv_obj_t* lblName = lv_label_create(row);
    if (!r.vendor.empty()) snprintf(buf, sizeof(buf), "%s %s", r.vendor.c_str(), r.name.c_str());
    else snprintf(buf, sizeof(buf), "%s", r.name.empty() ? "(unknown filament)" : r.name.c_str());
    lv_label_set_text(lblName, buf);
    lv_label_set_long_mode(lblName, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lblName, 430);
    lv_obj_set_pos(lblName, 56, 2);
    lv_obj_set_style_text_font(lblName, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lblName, lv_color_black(), 0);
    noClick(lblName);

    lv_obj_t* lblSub = lv_label_create(row);
    char idText[16];
    if (r.id < 0) snprintf(idText, sizeof(idText), "new");
    else snprintf(idText, sizeof(idText), "#%d", r.id);
    snprintf(buf, sizeof(buf), "%s%s%s  -  %s", r.material.c_str(), r.location.empty() ? "" : "  -  ",
             r.location.c_str(), idText);
    lv_label_set_text(lblSub, buf);
    lv_obj_set_pos(lblSub, 56, 28);
    lv_obj_set_style_text_font(lblSub, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblSub, lv_color_hex(0x1565C0), 0);
    noClick(lblSub);

    lv_obj_t* lblWeight = lv_label_create(row);
    if (r.hasRemaining) snprintf(buf, sizeof(buf), "%.0f g", r.remainingG);
    else snprintf(buf, sizeof(buf), "? g");
    lv_label_set_text(lblWeight, buf);
    lv_obj_set_align(lblWeight, LV_ALIGN_RIGHT_MID);
    lv_obj_set_x(lblWeight, -24);
    lv_obj_set_style_text_font(lblWeight, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lblWeight, lv_color_hex(0x333333), 0);
    noClick(lblWeight);

    if (r.pending) {   // an unsynced change touches this spool
        lv_obj_t* dot = lv_obj_create(row);
        lv_obj_set_size(dot, 12, 12);
        lv_obj_set_align(dot, LV_ALIGN_RIGHT_MID);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_set_style_bg_color(dot, lv_color_hex(PENDING_COLOR), 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        noClick(dot);
    }
}

void ScreenInventory::populate(const std::vector<SpoolRow>& rows, const char* statusText) {
    lv_obj_clean(list);
    lv_label_set_text(labelStatus, statusText);
    char buf[24];
    snprintf(buf, sizeof(buf), "%u spools", (unsigned)rows.size());
    lv_label_set_text(labelCount, buf);
    if (rows.empty()) {
        showMessage("No spools yet.\nSpools appear here after the first sync with Spoolman,\n"
                    "or when you write a tag and add it to inventory.", statusText);
        return;
    }
    for (const auto& r : rows) addSpoolRow(r);
}

void ScreenInventory::showMessage(const char* text, const char* statusText) {
    lv_obj_clean(list);
    lv_label_set_text(labelStatus, statusText);
    lv_obj_t* note = lv_label_create(list);
    lv_label_set_text(note, text);
    lv_obj_set_style_text_align(note, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(note, lv_color_hex(0x999999), 0);
    lv_obj_set_style_text_font(note, &lv_font_montserrat_16, 0);
    lv_obj_set_width(note, LV_PCT(100));
}

void ScreenInventory::show() {
    lv_screen_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}
