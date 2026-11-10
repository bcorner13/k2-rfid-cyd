#include <filament_db.h> // Updated include path

#include <FS.h>
#include <LittleFS.h>
#include <esp_heap_caps.h> // Required for heap_caps_malloc, heap_caps_free
#include <StreamUtils.h>   // Include StreamUtils library
#include <psram_allocator.h>
#include <color_names.h>

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
       ArduinoJson v7 document in PSRAM. Without the allocator, its small
       blocks went to internal RAM: the 711 KB printer DB drove internal
       MinFree to 1.2 KB at boot.
       ---------------------------------------------------- */
    JsonDocument doc(psramJsonAllocator());

    // Keep only the fields read below; printer entries carry dozens more.
    JsonDocument filter(psramJsonAllocator());
    JsonObject fBase = filter["result"]["list"][0]["base"].to<JsonObject>();
    fBase["id"] = true;
    fBase["brand"] = true;
    fBase["name"] = true;
    fBase["meterialType"] = true;
    fBase["colors"] = true;
    fBase["mixColorInfoList"][0]["colorEn"] = true;    // colour names, matched by hex
    fBase["mixColorInfoList"][0]["hexValue"] = true;
    JsonObject fKv = filter["result"]["list"][0]["kvParam"].to<JsonObject>();
    fKv["nozzle_temperature"] = true;
    fKv["hot_plate_temp"] = true;

    /* ----------------------------------------------------
       Temporarily remove task from WDT
       ---------------------------------------------------- */
    TaskHandle_t thisTask = xTaskGetCurrentTaskHandle();
    esp_task_wdt_delete(thisTask);

    Serial.println("Starting JSON deserialization...");
    uint32_t t0 = millis();

    DeserializationError error =
        deserializeJson(doc, file,
            DeserializationOption::Filter(filter),
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
    size_t skippedDup = 0;

    for (JsonObject obj : arr) {
        JsonObject base = obj["base"];
        JsonObject kv   = obj["kvParam"];

        // The tag carries only the 5-digit id, so keep one profile per id: the first one.
        // The printer's live DB appends user slicer presets under an existing id
        // (e.g. "CR-Silk @Creality K2 Plus 0.4 nozzle - Safer" = 05001 again).
        String id = base["id"] | "";
        bool dup = false;
        for (const auto& c : cache) if (c.id == id) { dup = true; break; }
        if (dup) { skippedDup++; continue; }

        FilamentProfile profile;
        profile.id            = id;
        profile.brand         = base["brand"] | "";
        profile.name          = base["name"] | "";
        profile.material_type = base["meterialType"] | "";
        profile.material_type.trim();

        JsonArray colors = base["colors"];
        JsonArrayConst mix = base["mixColorInfoList"];
        for (JsonVariant c : colors) {
            const char* s = c.as<const char*>();
            if (!s || s[0] != '#') continue;
            const uint32_t rgb = strtoul(s + 1, nullptr, 16);
            profile.colors.push_back(rgb);
            profile.color_names.push_back(String(::productColorName(mix, rgb).c_str()));
        }
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
    Serial.printf("Loaded %u filaments successfully (%u duplicate ids skipped).\n",
                  (unsigned)cache.size(), (unsigned)skippedDup);
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

String FilamentDB::getProductOptionsForDropdown(const String& brand, const String& type) const {
    String result;
    for (const auto& p : cache) {
        if (p.brand != brand || p.material_type != type || p.name.isEmpty()) continue;
        if (result.length()) result += "\n";
        result += p.name;
    }
    return result;
}

String FilamentDB::productColorName(const String& id, uint32_t rgb) const {
    for (const auto& p : cache) {
        if (p.id != id) continue;
        for (size_t i = 0; i < p.colors.size() && i < p.color_names.size(); i++)
            if (p.colors[i] == rgb) return p.color_names[i];
        return "";
    }
    return "";
}
