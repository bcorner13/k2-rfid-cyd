#pragma once
/**
 * @file spoolman_mirror_parse.h
 * @brief Pure parsing of Spoolman filament/spool list pages into local-mirror rows.
 *
 * Pages come from `GET /api/v1/filament?limit=50&offset=N` and
 * `GET /api/v1/spool?allow_archived=true&limit=50&offset=N` (Spoolman v0.27.1; null fields omitted).
 * Extra-field values are JSON-encoded strings, e.g. `"cfs_material_id": "\"08001\""`.
 * No Arduino dependencies: tested under `pio test -e native`.
 * Design: docs/superpowers/specs/2026-11-09-offline-inventory-design.md
 */
#include <ArduinoJson.h>
#include <spoolman_parse.h>
#include <cctype>
#include <cstddef>
#include <string>
#include <vector>

namespace spoolman {

struct MirrorFilament {
    int id = 0;
    int vendorId = 0;
    std::string vendor, name, material;
    std::string colorHex;                  // "RRGGBB" uppercase, "" if none
    std::vector<std::string> multiColors;  // each "RRGGBB"; empty unless 2+
    std::string direction;                 // "coaxial" | "longitudinal" | ""
    std::string cfsId;                     // decoded extra.cfs_material_id, "" if absent
    float weightG = 0.0f;
    int nozzleC = 0;
    int bedC = 0;
};

struct MirrorSpool {
    int id = 0;
    int filamentId = 0;
    float initialG = 0.0f;
    bool hasRemaining = false;
    float remainingG = 0.0f;
    std::string location;
    bool archived = false;
    std::string changeKey;                 // decoded extra.k2rfid_change
    std::vector<std::string> uids;         // normalized (normalizeUid)
};

enum class PageResult : uint8_t { Ok, Malformed };

/// Strips '#', uppercases, keeps the first 6 hex digits (RRGGBBAA -> RRGGBB). "" if invalid.
inline std::string normalizeHex(const char* in) {
    if (!in) return "";
    if (*in == '#') ++in;
    std::string out;
    for (; *in && out.size() < 6; ++in) {
        const unsigned char c = static_cast<unsigned char>(*in);
        if (!std::isxdigit(c)) return "";
        out.push_back(static_cast<char>(std::toupper(c)));
    }
    return out.size() == 6 ? out : "";
}

/// Spoolman stores extra values as JSON text: "\"08001\"" -> "08001". Anything but a JSON string -> "".
inline std::string decodeExtraString(const char* raw) {
    if (!raw || !*raw) return "";
    JsonDocument doc;
    if (deserializeJson(doc, raw)) return "";
    if (!doc.is<const char*>()) return "";
    return doc.as<const char*>();
}

namespace detail {
inline std::vector<std::string> splitHexList(const char* list) {
    std::vector<std::string> out;
    if (!list) return out;
    std::string item;
    for (const char* p = list;; ++p) {
        if (*p == ',' || *p == '\0') {
            std::string hex = normalizeHex(item.c_str());
            if (!hex.empty()) out.push_back(hex);
            item.clear();
            if (*p == '\0') break;
        } else if (*p != ' ') {
            item.push_back(*p);
        }
    }
    return out;
}

// Parses into a temporary array first, so a malformed page never touches `out`.
inline bool parseArray(const char* json, size_t len, const JsonDocument& filter, JsonDocument& doc) {
    if (deserializeJson(doc, json, len, DeserializationOption::Filter(filter))) return false;
    return doc.is<JsonArray>();
}
}  // namespace detail

/// Appends one page of filaments to `out`. `count` = items on this page (caller stops when < limit).
inline PageResult parseFilamentPage(const char* json, size_t len, std::vector<MirrorFilament>& out, size_t& count) {
    JsonDocument filter;
    JsonObject f = filter[0].to<JsonObject>();
    f["id"] = true;
    f["name"] = true;
    f["material"] = true;
    f["weight"] = true;
    f["color_hex"] = true;
    f["multi_color_hexes"] = true;
    f["multi_color_direction"] = true;
    f["settings_extruder_temp"] = true;
    f["settings_bed_temp"] = true;
    f["vendor"]["id"] = true;
    f["vendor"]["name"] = true;
    f["extra"]["cfs_material_id"] = true;

    JsonDocument doc;
    if (!detail::parseArray(json, len, filter, doc)) return PageResult::Malformed;

    JsonArray items = doc.as<JsonArray>();
    count = items.size();
    for (JsonObject item : items) {
        MirrorFilament m;
        m.id = item["id"] | 0;
        m.vendorId = item["vendor"]["id"] | 0;
        m.vendor = item["vendor"]["name"] | "";
        m.name = item["name"] | "";
        m.material = item["material"] | "";
        m.weightG = item["weight"] | 0.0f;
        m.nozzleC = item["settings_extruder_temp"] | 0;
        m.bedC = item["settings_bed_temp"] | 0;
        m.multiColors = detail::splitHexList(item["multi_color_hexes"] | "");
        if (m.multiColors.size() >= 2) {
            m.direction = item["multi_color_direction"] | "";
        } else {
            m.multiColors.clear();
            m.colorHex = normalizeHex(item["color_hex"] | "");
        }
        m.cfsId = decodeExtraString(item["extra"]["cfs_material_id"] | "");
        out.push_back(std::move(m));
    }
    return PageResult::Ok;
}

/// Appends one page of spools to `out`. `count` = items on this page (caller stops when < limit).
inline PageResult parseSpoolPage(const char* json, size_t len, std::vector<MirrorSpool>& out, size_t& count) {
    JsonDocument filter;
    JsonObject f = filter[0].to<JsonObject>();
    f["id"] = true;
    f["filament"]["id"] = true;
    f["initial_weight"] = true;
    f["remaining_weight"] = true;
    f["location"] = true;
    f["archived"] = true;
    f["extra"]["k2rfid_change"] = true;
    f["tags"][0]["uid"] = true;

    JsonDocument doc;
    if (!detail::parseArray(json, len, filter, doc)) return PageResult::Malformed;

    JsonArray items = doc.as<JsonArray>();
    count = items.size();
    for (JsonObject item : items) {
        MirrorSpool m;
        m.id = item["id"] | 0;
        m.filamentId = item["filament"]["id"] | 0;
        m.initialG = item["initial_weight"] | 0.0f;
        m.hasRemaining = item["remaining_weight"].is<float>();
        if (m.hasRemaining) m.remainingG = item["remaining_weight"].as<float>();
        m.location = item["location"] | "";
        m.archived = item["archived"] | false;
        m.changeKey = decodeExtraString(item["extra"]["k2rfid_change"] | "");
        for (JsonObject tag : item["tags"].as<JsonArray>()) {
            char uid[UID_BUF];
            if (normalizeUid(tag["uid"] | "", uid, sizeof(uid))) m.uids.emplace_back(uid);
        }
        out.push_back(std::move(m));
    }
    return PageResult::Ok;
}

}  // namespace spoolman
