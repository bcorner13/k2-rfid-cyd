#pragma once

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN

#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

/* Memory manager configuration */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (128U * 1024U) // 128KB
#define LV_MEM_ADR 0
#define LV_MEM_BUF_MAX_NUM 16

/* Hardware and Driver configuration */
#define LV_DISP_DEF_REFR_PERIOD 30      /*[ms]*/
#define LV_INDEV_DEF_READ_PERIOD 30     /*[ms]*/
#define LV_DPI_DEF 130     /*[px/inch]*/

/* Drawing configuration */
#define LV_DRAW_COMPLEX 1
#define LV_SHADOW_CACHE_SIZE 0
#define LV_IMG_CACHE_DEF_SIZE 0

/* Font configuration */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* Other feature configurations */
#define LV_USE_GPU_STM32_DMA2D 0
#define LV_USE_GPU_NXP_PXP 0
#define LV_USE_GPU_NXP_VG_LITE 0
#define LV_USE_GPU_SDL 0

#define LV_USE_GRID 1
#define LV_USE_FLEX 1
#define LV_USE_SNAPSHOT 1
#define LV_USE_OBSERVER 1
#define LV_USE_EVENT 1
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
/* New LVGL9 options */
#define LV_USE_DISPLAY 1
#define LV_USE_DRAW_BUF 1