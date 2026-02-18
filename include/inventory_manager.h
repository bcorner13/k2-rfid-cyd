#pragma once
/**
 * @file inventory_manager.h
 * @brief Spool inventory persistence with atomic save (FSD Section 6.3/6.5).
 *
 * SpoolRecord holds all per-spool data (profile snapshot, weight tracking,
 * status). InventoryManager provides CRUD operations backed by atomic
 * JSON writes to /inventory.json on LittleFS.
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <filament_profile.h>

enum class SpoolStatus : uint8_t {
    ACTIVE,
    EMPTY,
    ARCHIVED
};

enum class SpoolSource : uint8_t {
    SCAN,
    LIBRARY,
    MANUAL
};

struct WeightEntry {
    uint32_t weight_g;
    uint32_t timestamp;  // uptime seconds (no RTC)
};

struct SpoolRecord {
    String spool_id;     // "SPL-NNNN"
    String tag_uid;      // empty if local-only

    // Inline profile snapshot
    String brand;
    String name;
    String material_type;
    String color_hex;    // 6 hex chars, e.g. "FF8800"
    uint32_t diameter_um = 1750;
    uint16_t nozzle_temp_min = 0;
    uint16_t nozzle_temp_max = 0;
    uint16_t bed_temp_min = 0;
    uint16_t bed_temp_max = 0;
    uint16_t print_speed_min = 0;
    uint16_t print_speed_max = 0;
    uint8_t fan_percent = 100;
    float density = 1.24f;

    uint32_t initial_weight_g = 1000;
    uint32_t current_weight_g = 1000;

    SpoolStatus status = SpoolStatus::ACTIVE;
    SpoolSource source = SpoolSource::LIBRARY;

    uint32_t created_at = 0;   // uptime seconds
    uint32_t updated_at = 0;   // uptime seconds

    std::vector<WeightEntry> weight_history;
};

static constexpr size_t MAX_SPOOLS = 100;

class InventoryManager {
public:
    bool init();
    bool save();

    /** Add a spool from a FilamentProfile. Returns spool_id or empty on failure. */
    String addSpool(const FilamentProfile& profile, SpoolSource source, uint32_t weight_g = 1000);

    bool archiveSpool(const String& spool_id);
    bool deleteSpool(const String& spool_id);
    bool updateWeight(const String& spool_id, uint32_t new_weight_g);

    const SpoolRecord* getSpoolByUID(const String& tag_uid) const;
    const SpoolRecord* getSpoolById(const String& spool_id) const;
    std::vector<const SpoolRecord*> getAllActive() const;
    size_t getSpoolCount() const { return _spools.size(); }

private:
    bool load();
    int findIndex(const String& spool_id) const;

    std::vector<SpoolRecord> _spools;
    uint32_t _nextId = 1;
    bool _dirty = false;
};

extern InventoryManager inventory;
