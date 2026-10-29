#pragma once

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN

/* Memory manager — LVGL 9.x API.
 * LV_STDLIB_CUSTOM (255): LVGL objects/styles are allocated from PSRAM by
 * src/lv_mem_psram.c. Plain malloc (LV_STDLIB_CLIB) put every small LVGL
 * allocation in internal RAM — ~400 Library grid objects for 66 filaments
 * left only ~2KB internal heap free. The display draw buffer stays in internal
 * RAM (heap_caps_malloc in lvgl_display.cpp) for render speed. */
#define LV_USE_STDLIB_MALLOC 255  /* LV_STDLIB_CUSTOM */

/* Hardware and Driver configuration */
#define LV_DEF_REFR_PERIOD  30      /*[ms]*/
#define LV_DPI_DEF 130              /*[px/inch]*/

/* Font configuration */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* Widget / feature flags */
#define LV_USE_GRID 1
#define LV_USE_FLEX 1
#define LV_USE_SNAPSHOT 1
#define LV_USE_OBSERVER 1
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
