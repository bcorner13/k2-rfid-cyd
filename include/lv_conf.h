#pragma once

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN

/* Memory manager — LVGL 9.5 API.
 * LV_STDLIB_CLIB (1): use standard malloc/free/realloc from the system heap.
 * On ESP32 Arduino this gives ~200KB+ of dynamic heap instead of LVGL's
 * default 64KB built-in pool. Display buffer is still PSRAM via heap_caps_malloc
 * in lvgl_display.cpp. The old LV_MEM_CUSTOM block is unused in LVGL 9.x. */
#define LV_USE_STDLIB_MALLOC 1  /* LV_STDLIB_CLIB */

/* Hardware and Driver configuration */
#define LV_DEF_REFR_PERIOD  30      /*[ms]*/
#define LV_DPI_DEF 130              /*[px/inch]*/

/* Font configuration */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
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
