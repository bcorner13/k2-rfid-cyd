/**
 * @file lv_mem_psram.c
 * @brief LVGL custom allocator (LV_STDLIB_CUSTOM) backed by PSRAM.
 *
 * Keeps LVGL's object/style allocations out of the ~320KB internal heap so
 * WiFi, I2C and FreeRTOS have headroom. Falls back to internal RAM if a PSRAM
 * allocation fails.
 */
#include <lvgl.h>

#if LV_USE_STDLIB_MALLOC == LV_STDLIB_CUSTOM

#include <esp_heap_caps.h>

#define LV_PSRAM_CAPS (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)

void lv_mem_init(void) {}

void lv_mem_deinit(void) {}

lv_mem_pool_t lv_mem_add_pool(void * mem, size_t bytes)
{
    LV_UNUSED(mem);
    LV_UNUSED(bytes);
    return NULL;
}

void lv_mem_remove_pool(lv_mem_pool_t pool)
{
    LV_UNUSED(pool);
}

void * lv_malloc_core(size_t size)
{
    void * p = heap_caps_malloc(size, LV_PSRAM_CAPS);
    if (p == NULL) p = heap_caps_malloc(size, MALLOC_CAP_8BIT);
    return p;
}

void * lv_realloc_core(void * p, size_t new_size)
{
    void * np = heap_caps_realloc(p, new_size, LV_PSRAM_CAPS);
    if (np == NULL && new_size > 0) np = heap_caps_realloc(p, new_size, MALLOC_CAP_8BIT);
    return np;
}

void lv_free_core(void * p)
{
    heap_caps_free(p);
}

void lv_mem_monitor_core(lv_mem_monitor_t * mon_p)
{
    multi_heap_info_t info;
    heap_caps_get_info(&info, LV_PSRAM_CAPS);

    mon_p->total_size = info.total_free_bytes + info.total_allocated_bytes;
    mon_p->free_size = info.total_free_bytes;
    mon_p->free_biggest_size = info.largest_free_block;
    mon_p->free_cnt = info.free_blocks;
    mon_p->used_cnt = info.allocated_blocks;
    mon_p->max_used = mon_p->total_size - info.minimum_free_bytes;
    mon_p->used_pct = mon_p->total_size
        ? (uint8_t)(100 - (100 * info.total_free_bytes) / mon_p->total_size) : 0;
    mon_p->frag_pct = info.total_free_bytes
        ? (uint8_t)(100 - (100 * info.largest_free_block) / info.total_free_bytes) : 0;
}

lv_result_t lv_mem_test_core(void)
{
    return heap_caps_check_integrity(LV_PSRAM_CAPS, false) ? LV_RESULT_OK : LV_RESULT_INVALID;
}

#endif /* LV_STDLIB_CUSTOM */
