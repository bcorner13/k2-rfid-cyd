#include <inventory_manager.h>
#include <atomic_write.h>
#include <LittleFS.h>

static const char* INVENTORY_PATH = "/inventory.json";

InventoryManager inventory;

bool InventoryManager::init() {
    Serial.println("InventoryManager::init() called.");

    recoverTmpFile(INVENTORY_PATH);

    if (!LittleFS.exists(INVENTORY_PATH)) {
        Serial.println("No inventory.json found, starting empty.");
        _spools.clear();
        _nextId = 1;
        return save();
    }

    if (!load()) {
        Serial.println("WARNING: inventory.json load failed, starting fresh.");
        _spools.clear();
        _nextId = 1;
        return save();
    }

    return true;
}

bool InventoryManager::load() {
    File file = LittleFS.open(INVENTORY_PATH, "r");
    if (!file) {
        Serial.println("ERROR: Failed to open inventory.json for reading!");
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.printf("ERROR: Failed to parse inventory.json: %s\n", error.c_str());
        return false;
    }

    _nextId = doc["next_id"] | 1;
    _spools.clear();

    JsonArray arr = doc["spools"].as<JsonArray>();
    if (arr.isNull()) {
        Serial.println("inventory.json has no spools array, starting empty.");
        return true;
    }

    _spools.reserve(arr.size());

    for (JsonObject obj : arr) {
        SpoolRecord rec;
        rec.spool_id       = obj["spool_id"] | "";
        rec.tag_uid        = obj["tag_uid"] | "";
        rec.brand          = obj["brand"] | "";
        rec.name           = obj["name"] | "";
        rec.material_type  = obj["material_type"] | "";
        rec.color_hex      = obj["color_hex"] | "FFFFFF";
        rec.diameter_um    = obj["diameter_um"] | 1750;
        rec.nozzle_temp_min = obj["nozzle_temp_min"] | 0;
        rec.nozzle_temp_max = obj["nozzle_temp_max"] | 0;
        rec.bed_temp_min   = obj["bed_temp_min"] | 0;
        rec.bed_temp_max   = obj["bed_temp_max"] | 0;
        rec.print_speed_min = obj["print_speed_min"] | 0;
        rec.print_speed_max = obj["print_speed_max"] | 0;
        rec.fan_percent    = obj["fan_percent"] | 100;
        rec.density        = obj["density"] | 1.24f;

        rec.initial_weight_g = obj["initial_weight_g"] | 1000;
        rec.current_weight_g = obj["current_weight_g"] | 1000;

        uint8_t status_val = obj["status"] | 0;
        rec.status = static_cast<SpoolStatus>(status_val);

        uint8_t source_val = obj["source"] | 1;
        rec.source = static_cast<SpoolSource>(source_val);

        rec.created_at = obj["created_at"] | 0;
        rec.updated_at = obj["updated_at"] | 0;

        JsonArray hist = obj["weight_history"].as<JsonArray>();
        if (!hist.isNull()) {
            rec.weight_history.reserve(hist.size());
            for (JsonObject entry : hist) {
                WeightEntry we;
                we.weight_g  = entry["weight_g"] | 0;
                we.timestamp = entry["timestamp"] | 0;
                rec.weight_history.push_back(we);
            }
        }

        _spools.push_back(std::move(rec));
        yield();
    }

    Serial.printf("Loaded %u spools from inventory.\n", _spools.size());
    return true;
}

bool InventoryManager::save() {
    JsonDocument doc;

    doc["next_id"] = _nextId;
    JsonArray arr = doc["spools"].to<JsonArray>();

    for (const auto& rec : _spools) {
        JsonObject obj = arr.add<JsonObject>();
        obj["spool_id"]       = rec.spool_id;
        obj["tag_uid"]        = rec.tag_uid;
        obj["brand"]          = rec.brand;
        obj["name"]           = rec.name;
        obj["material_type"]  = rec.material_type;
        obj["color_hex"]      = rec.color_hex;
        obj["diameter_um"]    = rec.diameter_um;
        obj["nozzle_temp_min"] = rec.nozzle_temp_min;
        obj["nozzle_temp_max"] = rec.nozzle_temp_max;
        obj["bed_temp_min"]   = rec.bed_temp_min;
        obj["bed_temp_max"]   = rec.bed_temp_max;
        obj["print_speed_min"] = rec.print_speed_min;
        obj["print_speed_max"] = rec.print_speed_max;
        obj["fan_percent"]    = rec.fan_percent;
        obj["density"]        = rec.density;
        obj["initial_weight_g"] = rec.initial_weight_g;
        obj["current_weight_g"] = rec.current_weight_g;
        obj["status"]         = static_cast<uint8_t>(rec.status);
        obj["source"]         = static_cast<uint8_t>(rec.source);
        obj["created_at"]     = rec.created_at;
        obj["updated_at"]     = rec.updated_at;

        if (!rec.weight_history.empty()) {
            JsonArray hist = obj["weight_history"].to<JsonArray>();
            for (const auto& we : rec.weight_history) {
                JsonObject entry = hist.add<JsonObject>();
                entry["weight_g"]  = we.weight_g;
                entry["timestamp"] = we.timestamp;
            }
        }
    }

    bool ok = atomicWriteJson(INVENTORY_PATH, doc);
    if (ok) {
        _dirty = false;
        Serial.println("Inventory saved.");
    }
    return ok;
}

String InventoryManager::addSpool(const FilamentProfile& profile, SpoolSource source, uint32_t weight_g) {
    if (_spools.size() >= MAX_SPOOLS) {
        Serial.println("ERROR: Inventory full (100 spools max).");
        return "";
    }

    SpoolRecord rec;
    // Generate ID: SPL-0001, SPL-0002, etc.
    char id_buf[16];
    snprintf(id_buf, sizeof(id_buf), "SPL-%04u", _nextId++);
    rec.spool_id = id_buf;

    rec.brand         = profile.brand;
    rec.name          = profile.name;
    rec.material_type = profile.material_type;

    // Convert uint32_t color_hex to 6-char hex string
    char hex_buf[8];
    snprintf(hex_buf, sizeof(hex_buf), "%06X", profile.color_hex & 0xFFFFFF);
    rec.color_hex = hex_buf;

    rec.nozzle_temp_min = profile.nozzle_temp;
    rec.nozzle_temp_max = profile.nozzle_temp;
    rec.bed_temp_min    = profile.bed_temp;
    rec.bed_temp_max    = profile.bed_temp;

    rec.initial_weight_g = weight_g;
    rec.current_weight_g = weight_g;

    rec.status  = SpoolStatus::ACTIVE;
    rec.source  = source;

    uint32_t now = millis() / 1000;
    rec.created_at = now;
    rec.updated_at = now;

    // Initial weight history entry
    rec.weight_history.push_back({weight_g, now});

    String spool_id = rec.spool_id;
    _spools.push_back(std::move(rec));

    if (!save()) {
        Serial.println("ERROR: Failed to save after addSpool.");
    }

    return spool_id;
}

// P1.5: Add spool from fully-populated SpoolRecord (custom entry)
String InventoryManager::addSpoolRecord(SpoolRecord rec) {
    if (_spools.size() >= MAX_SPOOLS) {
        Serial.println("ERROR: Inventory full (100 spools max).");
        return "";
    }

    char id_buf[16];
    snprintf(id_buf, sizeof(id_buf), "SPL-%04u", _nextId++);
    rec.spool_id = id_buf;

    uint32_t now = millis() / 1000;
    rec.created_at = now;
    rec.updated_at = now;

    rec.status = SpoolStatus::ACTIVE;

    if (rec.weight_history.empty()) {
        rec.weight_history.push_back({rec.current_weight_g, now});
    }

    String spool_id = rec.spool_id;
    _spools.push_back(std::move(rec));

    if (!save()) {
        Serial.println("ERROR: Failed to save after addSpoolRecord.");
    }

    return spool_id;
}

// P1.8: Archive — soft delete, retains tag_uid for reactivation
bool InventoryManager::archiveSpool(const String& spool_id) {
    int idx = findIndex(spool_id);
    if (idx < 0) return false;

    _spools[idx].status = SpoolStatus::ARCHIVED;
    _spools[idx].updated_at = millis() / 1000;
    // tag_uid intentionally retained (FSD 6.9)
    Serial.printf("Archived spool %s (tag_uid retained: %s)\n",
                  spool_id.c_str(), _spools[idx].tag_uid.c_str());
    return save();
}

// P1.8: Delete — hard delete, clears tag_uid so tag becomes unrecognized
bool InventoryManager::deleteSpool(const String& spool_id) {
    int idx = findIndex(spool_id);
    if (idx < 0) return false;

    Serial.printf("Deleting spool %s (tag_uid cleared)\n", spool_id.c_str());
    _spools.erase(_spools.begin() + idx);
    return save();
}

bool InventoryManager::linkTagUID(const String& spool_id, const String& tag_uid) {
    int idx = findIndex(spool_id);
    if (idx < 0) return false;

    _spools[idx].tag_uid = tag_uid;
    _spools[idx].updated_at = millis() / 1000;
    Serial.printf("Linked tag %s to spool %s\n", tag_uid.c_str(), spool_id.c_str());
    return save();
}

// P1.7: Unlink tag from spool (FSD 6.7 "Unlink Tag" action)
bool InventoryManager::unlinkTag(const String& spool_id) {
    int idx = findIndex(spool_id);
    if (idx < 0) return false;

    Serial.printf("Unlinked tag %s from spool %s\n",
                  _spools[idx].tag_uid.c_str(), spool_id.c_str());
    _spools[idx].tag_uid = "";
    _spools[idx].updated_at = millis() / 1000;
    return save();
}

// P1.7: Reactivate an archived spool (FSD 6.7 "Archived spool rescanned")
bool InventoryManager::reactivateSpool(const String& spool_id) {
    int idx = findIndex(spool_id);
    if (idx < 0) return false;

    if (_spools[idx].status != SpoolStatus::ARCHIVED) {
        Serial.printf("Spool %s is not archived (status=%u)\n",
                      spool_id.c_str(), static_cast<uint8_t>(_spools[idx].status));
        return false;
    }

    _spools[idx].status = SpoolStatus::ACTIVE;
    _spools[idx].updated_at = millis() / 1000;
    Serial.printf("Reactivated spool %s\n", spool_id.c_str());
    return save();
}

// P1.7: Check if UID is assigned to any active (non-archived) spool
String InventoryManager::checkUIDCollision(const String& tag_uid) const {
    if (tag_uid.isEmpty()) return "";

    for (const auto& rec : _spools) {
        if (rec.tag_uid == tag_uid && rec.status != SpoolStatus::ARCHIVED) {
            return rec.spool_id;
        }
    }
    return "";
}

bool InventoryManager::updateWeight(const String& spool_id, uint32_t new_weight_g) {
    int idx = findIndex(spool_id);
    if (idx < 0) return false;

    uint32_t now = millis() / 1000;
    _spools[idx].current_weight_g = new_weight_g;
    _spools[idx].updated_at = now;
    _spools[idx].weight_history.push_back({new_weight_g, now});

    if (new_weight_g == 0) {
        _spools[idx].status = SpoolStatus::EMPTY;
    }

    return save();
}

// P1.7: Search active spools first, then archived (FSD 6.7 lookup order)
const SpoolRecord* InventoryManager::getSpoolByUID(const String& tag_uid) const {
    if (tag_uid.isEmpty()) return nullptr;

    // 1. Search active spools (ACTIVE or EMPTY)
    for (const auto& rec : _spools) {
        if (rec.tag_uid == tag_uid && rec.status != SpoolStatus::ARCHIVED) {
            return &rec;
        }
    }

    // 2. Search archived spools
    for (const auto& rec : _spools) {
        if (rec.tag_uid == tag_uid && rec.status == SpoolStatus::ARCHIVED) {
            return &rec;
        }
    }

    return nullptr;
}

const SpoolRecord* InventoryManager::getArchivedSpoolByUID(const String& tag_uid) const {
    if (tag_uid.isEmpty()) return nullptr;

    for (const auto& rec : _spools) {
        if (rec.tag_uid == tag_uid && rec.status == SpoolStatus::ARCHIVED) {
            return &rec;
        }
    }
    return nullptr;
}

const SpoolRecord* InventoryManager::getSpoolById(const String& spool_id) const {
    for (const auto& rec : _spools) {
        if (rec.spool_id == spool_id) return &rec;
    }
    return nullptr;
}

std::vector<const SpoolRecord*> InventoryManager::getAllActive() const {
    std::vector<const SpoolRecord*> result;
    for (const auto& rec : _spools) {
        if (rec.status != SpoolStatus::ARCHIVED) {
            result.push_back(&rec);
        }
    }
    return result;
}

int InventoryManager::findIndex(const String& spool_id) const {
    for (size_t i = 0; i < _spools.size(); i++) {
        if (_spools[i].spool_id == spool_id) return static_cast<int>(i);
    }
    return -1;
}
