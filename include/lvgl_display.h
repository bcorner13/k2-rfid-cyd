#pragma once

#include <lvgl.h> // REQUIRED

void my_touch_read(lv_indev_t *indev, lv_indev_data_t *data);
void lvgl_display_init();
void lvgl_display_start_ui();
void splash_add_status(const char* text, bool initial_state);
void splash_update_status(int index, const char* status_text, bool success);

