#pragma once
// LVGL custom allocator: tries PSRAM first, falls back to internal SRAM.
#include <esp_heap_caps.h>
#include <stddef.h>

static inline void* lv_custom_alloc(size_t size) {
    void* p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    return p;
}

static inline void lv_custom_free(void* ptr) {
    heap_caps_free(ptr);
}

static inline void* lv_custom_realloc(void* ptr, size_t size) {
    void* p = heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = heap_caps_realloc(ptr, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    return p;
}
