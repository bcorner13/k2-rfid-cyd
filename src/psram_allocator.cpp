#include <psram_allocator.h>
#include <Arduino.h>
#include <esp_heap_caps.h>

// Custom allocator to force memory into PSRAM
struct SpiRamAllocator : ArduinoJson::Allocator {
  void* allocate(size_t size) override {
    void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (ptr == nullptr) {
        Serial.printf("ERROR: PSRAM Alloc FAILED for %u bytes! Free PSRAM: %u\n", size, heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    }
    return ptr;
  }
  void deallocate(void* pointer) override {
    heap_caps_free(pointer);
  }
  void* reallocate(void* ptr, size_t new_size) override {
    void* new_ptr = heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM);
    if (new_ptr == nullptr) {
        Serial.printf("ERROR: PSRAM Realloc FAILED for %u bytes (from %p)! Free PSRAM: %u\n", new_size, ptr, heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    }
    return new_ptr;
  }
  virtual ~SpiRamAllocator() = default; // Added virtual destructor
};

ArduinoJson::Allocator* psramJsonAllocator() {
    static SpiRamAllocator instance;
    return &instance;
}
