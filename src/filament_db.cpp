#include <filament_db.h> // Updated include path

#include <FS.h>
#include <LittleFS.h>
#include <esp_heap_caps.h> // Required for heap_caps_malloc, heap_caps_free
#include <StreamUtils.h>   // Include StreamUtils library

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

// Declare a static instance of the allocator
static SpiRamAllocator psramAllocator;

FilamentDB filamentDB;

FilamentDB::FilamentDB() = default;

bool FilamentDB::init() {
    Serial.println("FilamentDB::init() called.");

    // --- PSRAM Check ---
    if (!psramFound()) {
        Serial.println("WARNING: PSRAM not found or not initialized!");
    } else {
        Serial.println("PSRAM found and should be used by allocator.");
    }
    // --- End PSRAM Check ---

    if (!LittleFS.begin(true)) {
        Serial.println("ERROR: LittleFS Mount Failed!");
        return false;
    }
    Serial.println("LittleFS Mounted Successfully.");
    return loadDatabase();
}
#include "esp_task_wdt.h"

bool FilamentDB::loadDatabase() {
    Serial.println("\nFilamentDB::loadDatabase() called.");

    File file = LittleFS.open("/material_database.json", "r");
    if (!file) {
        Serial.println("ERROR: Failed to open material_database.json!");
        return false;
    }

    Serial.printf("JSON file size: %u bytes\n", file.size());
    Serial.printf("Free heap (internal): %u bytes\n", ESP.getFreeHeap());
    Serial.printf("Free PSRAM: %u bytes\n",
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    /* ----------------------------------------------------
       ArduinoJson v7 document (NO custom allocator)
       ESP32 will allocate from PSRAM automatically
       ---------------------------------------------------- */
    JsonDocument doc;

    /* ----------------------------------------------------
       Temporarily remove task from WDT
       ---------------------------------------------------- */
    TaskHandle_t thisTask = xTaskGetCurrentTaskHandle();
    esp_task_wdt_delete(thisTask);

    Serial.println("Starting JSON deserialization...");
    uint32_t t0 = millis();

    DeserializationError error =
        deserializeJson(doc, file,
            DeserializationOption::NestingLimit(20));

    uint32_t dt = millis() - t0;

    esp_task_wdt_add(thisTask);

    Serial.printf("Deserialization completed in %u ms\n", dt);

    if (error) {
        Serial.print("ERROR: JSON parse failed: ");
        Serial.println(error.c_str());
        file.close();
        return false;
    }

    JsonArray arr = doc["result"]["list"].as<JsonArray>();
    if (arr.isNull()) {
        Serial.println("ERROR: 'result.list' missing or invalid");
        file.close();
        return false;
    }

    cache.clear();
    cache.reserve(arr.size());

    for (JsonObject obj : arr) {
        JsonObject base = obj["base"];
        JsonObject kv   = obj["kvParam"];

        FilamentProfile profile;
        profile.id            = base["id"] | "";
        profile.brand         = base["brand"] | "";
        profile.name          = base["name"] | "";
        profile.material_type = base["meterialType"] | "";
        profile.material_type.trim();

        JsonArray colors = base["colors"];
        if (!colors.isNull() && colors.size() > 0) {
            profile.color_name = colors[0].as<String>();
            if (profile.color_name.startsWith("#")) {
                profile.color_hex =
                    strtol(profile.color_name.substring(1).c_str(), nullptr, 16);
            } else {
                profile.color_hex = 0xFFFFFF;
            }
        } else {
            profile.color_name = "#FFFFFF";
            profile.color_hex  = 0xFFFFFF;
        }

        profile.nozzle_temp =
            kv["nozzle_temperature"].is<uint16_t>()
                ? kv["nozzle_temperature"].as<uint16_t>() : 0;

        profile.bed_temp =
            kv["hot_plate_temp"].is<uint16_t>()
                ? kv["hot_plate_temp"].as<uint16_t>() : 0;

        cache.push_back(profile);
        yield();
    }

    file.close();
    Serial.printf("Loaded %u filaments successfully.\n", cache.size());
    return true;
}


std::vector<FilamentProfile> FilamentDB::getAllFilaments() {
    return cache;
}

FilamentProfile FilamentDB::getProfileById(const String& id) const {
    FilamentProfile out;
    if (getProfileById(id, out)) return out;
    return {};
}

bool FilamentDB::getProfileById(const String& id, FilamentProfile& out) const {
    for (const auto& p : cache) {
        if (p.id == id) {
            out = p;
            return true;
        }
    }
    return false;
}

String FilamentDB::getBrandOptionsForDropdown() const {
    std::vector<String> seen;
    String result;
    for (const auto& p : cache) {
        if (p.brand.isEmpty()) continue;
        bool found = false;
        for (const auto& s : seen) if (s == p.brand) { found = true; break; }
        if (found) continue;
        seen.push_back(p.brand);
        if (result.length()) result += "\n";
        result += p.brand;
    }
    return result;
}

String FilamentDB::getMaterialTypeOptionsForDropdown() const {
    std::vector<String> seen;
    String result;
    for (const auto& p : cache) {
        if (p.material_type.isEmpty()) continue;
        bool found = false;
        for (const auto& s : seen) if (s == p.material_type) { found = true; break; }
        if (found) continue;
        seen.push_back(p.material_type);
        if (result.length()) result += "\n";
        result += p.material_type;
    }
    return result;
}