#include "../../../include/ui/widgets/spool_widget.h"

static const int SPOOL_SIZE = 250;   // outer diameter
static const int RING_MAX_WIDTH = 36; // max radial thickness of filament ring (profile view)
static const int CORE_SIZE = 24;     // center hub (spool core) diameter
static const int CONTAINER_HEIGHT = 340; // room for spool circle + labels below
static const int SPOOL_CENTER_Y = 170;   // center of circle in container (340/2 = 170 for top-half placement)
static const int SPOOL_RADIUS = 125;     // SPOOL_SIZE/2

void SpoolWidget::create(lv_obj_t* parent) {
    container = lv_obj_create(parent);
    lv_obj_set_size(container, 280, CONTAINER_HEIGHT);
    lv_obj_set_align(container, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(container, 0, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);

    // Border ring: light grey to black outline around the spool circle
    lv_obj_t* border_arc = lv_arc_create(container);
    lv_obj_set_size(border_arc, SPOOL_SIZE, SPOOL_SIZE);
    lv_arc_set_bg_angles(border_arc, 0, 360);
    lv_arc_set_value(border_arc, 360);
    lv_obj_set_align(border_arc, LV_ALIGN_CENTER);
    lv_obj_remove_style(border_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(border_arc, 3, LV_PART_MAIN);
    lv_obj_set_style_arc_color(border_arc, lv_color_hex(0x404040), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(border_arc, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(border_arc, LV_OPA_TRANSP, LV_PART_MAIN);

    // Colored filament ring (spool fill)
    filament = lv_arc_create(container);
    lv_obj_set_size(filament, SPOOL_SIZE, SPOOL_SIZE);
    lv_arc_set_bg_angles(filament, 0, 360);
    lv_arc_set_value(filament, 360);
    lv_obj_set_align(filament, LV_ALIGN_CENTER);
    lv_obj_remove_style(filament, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(filament, RING_MAX_WIDTH, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(filament, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(filament, LV_OPA_TRANSP, LV_PART_MAIN);

    // Horizontal and vertical lines for spool look (crosshairs at 12, 3, 6, 9 o'clock)
    const int cx = 140;
    const int cy = SPOOL_CENTER_Y;
    const int r = SPOOL_RADIUS - 4;
    static lv_point_precise_t line_h_pts[2];
    static lv_point_precise_t line_v_pts[2];
    lv_point_precise_set(&line_h_pts[0], (lv_value_precise_t)(cx - r), (lv_value_precise_t)cy);
    lv_point_precise_set(&line_h_pts[1], (lv_value_precise_t)(cx + r), (lv_value_precise_t)cy);
    lv_point_precise_set(&line_v_pts[0], (lv_value_precise_t)cx, (lv_value_precise_t)(cy - r));
    lv_point_precise_set(&line_v_pts[1], (lv_value_precise_t)cx, (lv_value_precise_t)(cy + r));
    lv_obj_t* line_h_obj = lv_line_create(container);
    lv_line_set_points(line_h_obj, line_h_pts, 2);
    lv_obj_set_size(line_h_obj, 280, CONTAINER_HEIGHT);
    lv_obj_set_style_line_width(line_h_obj, 2, LV_PART_MAIN);
    lv_obj_set_style_line_color(line_h_obj, lv_color_hex(0x606060), LV_PART_MAIN);
    lv_obj_set_pos(line_h_obj, 0, 0);
    lv_obj_t* line_v_obj = lv_line_create(container);
    lv_line_set_points(line_v_obj, line_v_pts, 2);
    lv_obj_set_size(line_v_obj, 280, CONTAINER_HEIGHT);
    lv_obj_set_style_line_width(line_v_obj, 2, LV_PART_MAIN);
    lv_obj_set_style_line_color(line_v_obj, lv_color_hex(0x606060), LV_PART_MAIN);
    lv_obj_set_pos(line_v_obj, 0, 0);

    // Center hub (spool core); tap opens color picker
    core = lv_obj_create(container);
    lv_obj_set_size(core, CORE_SIZE, CORE_SIZE);
    lv_obj_set_align(core, LV_ALIGN_CENTER);
    lv_obj_set_style_radius(core, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(core, lv_color_black(), 0);
    lv_obj_set_style_border_width(core, 0, 0);
    lv_obj_set_style_shadow_width(core, 0, 0);

    // Filament type and weight labels *below* the circle (fixed Y in bottom band)
    labelType = lv_label_create(container);
    lv_obj_set_style_text_font(labelType, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(labelType, lv_color_black(), 0);
    lv_obj_set_align(labelType, LV_ALIGN_TOP_MID);
    lv_obj_set_y(labelType, 298);

    labelWeight = lv_label_create(container);
    lv_obj_set_style_text_font(labelWeight, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(labelWeight, lv_color_black(), 0);
    lv_obj_set_align(labelWeight, LV_ALIGN_TOP_MID);
    lv_obj_set_y(labelWeight, 318);
}

void SpoolWidget::update(const char* type, uint32_t color, uint32_t weight) {
    lv_label_set_text(labelType, type);
    // Spool color fill: the filament ring uses the selected color
    lv_obj_set_style_arc_color(filament, lv_color_hex(color), LV_PART_INDICATOR);

    // Profile view: ring thickness = remaining (pic 2 half full, pic 3 almost empty, pic 4 almost full)
    uint32_t w = weight > 1000 ? 1000 : weight;
    int width = (int)((RING_MAX_WIDTH * w) / 1000);
    if (width < 0) width = 0;
    if (width > RING_MAX_WIDTH) width = RING_MAX_WIDTH;
    lv_obj_set_style_arc_width(filament, width, LV_PART_INDICATOR);
    lv_arc_set_value(filament, 360);

    lv_label_set_text_fmt(labelWeight, "%dg", weight);
}