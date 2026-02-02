#include "../../../include/ui/widgets/spool_widget.h"

void SpoolWidget::create(lv_obj_t* parent) {
    container = lv_obj_create(parent);
    lv_obj_set_size(container, 280, 280);
    lv_obj_set_align(container, LV_ALIGN_CENTER); // Changed from lv_obj_center
    lv_obj_set_style_bg_opa(container, 0, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);

    // Background Arc
    lv_obj_t* bg_arc = lv_arc_create(container);
    lv_obj_set_size(bg_arc, 250, 250);
    lv_arc_set_bg_angles(bg_arc, 0, 360);
    lv_arc_set_value(bg_arc, 360);
    lv_obj_set_align(bg_arc, LV_ALIGN_CENTER); // Changed from lv_obj_center
    lv_obj_remove_style(bg_arc, NULL, LV_PART_KNOB);
    // Replaced lv_palette_lighten with lv_color_lighten and lv_color_make
    lv_obj_set_style_arc_color(bg_arc, lv_color_lighten(lv_color_make(128, 128, 128), 20), LV_PART_MAIN);
    lv_obj_set_style_arc_width(bg_arc, 30, LV_PART_MAIN);

    // Filament Arc
    filament = lv_arc_create(container);
    lv_obj_set_size(filament, 250, 250);
    lv_arc_set_bg_angles(filament, 0, 360);
    lv_arc_set_rotation(filament, 270);
    lv_obj_set_align(filament, LV_ALIGN_CENTER); // Changed from lv_obj_center
    lv_obj_remove_style(filament, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(filament, 30, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(filament, true, LV_PART_INDICATOR);

    // Type Label (e.g., "PLA")
    labelType = lv_label_create(container);
    lv_obj_set_style_text_font(labelType, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(labelType, lv_color_black(), 0);
    lv_obj_set_align(labelType, LV_ALIGN_CENTER); // Changed from lv_obj_center

    // Weight Label (e.g., "1000g")
    labelWeight = lv_label_create(container);
    lv_obj_set_style_text_font(labelWeight, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(labelWeight, lv_color_black(), 0);
    lv_obj_set_align(labelWeight, LV_ALIGN_CENTER); // Changed from lv_obj_align
    lv_obj_set_y(labelWeight, 40); // Set y offset
}

void SpoolWidget::update(const char* type, uint32_t color, uint32_t weight) {
    lv_label_set_text(labelType, type);
    lv_obj_set_style_arc_color(filament, lv_color_hex(color), LV_PART_INDICATOR);

    int32_t percentage = (int32_t)((weight / 1000.0f) * 360);
    lv_arc_set_value(filament, percentage);

    lv_label_set_text_fmt(labelWeight, "%dg", weight);
}