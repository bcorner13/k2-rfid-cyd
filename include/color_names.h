#pragma once
/**
 * @file color_names.h
 * @brief Human names for filament colours. Pure; host-tested (test_color_names).
 *
 * Tags and Spoolman store hex only. Product colours get their name from
 * material_database.json (base.mixColorInfoList[].colorEn, matched by hexValue). Anything else
 * gets the nearest of the picker palette's names (include/ui/color_palette.h), shown with "~"
 * unless it is that palette colour exactly.
 */
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <string>
#include <ArduinoJson.h>

namespace color_names_detail {
struct Named { const char* name; uint32_t rgb; };
// The 25 picker palette colours (same order as FILAMENT_COLORS), plus Beige.
static const Named kNamed[] = {
    {"White", 0xFFFFFF},  {"Black", 0x000000},      {"Red", 0xFF0000},        {"Bright Green", 0x00FF00},
    {"Blue", 0x0000FF},   {"Yellow", 0xFFFF00},     {"Cyan", 0x00FFFF},       {"Magenta", 0xFF00FF},
    {"Grey", 0x808080},   {"Orange", 0xFFA500},     {"Maroon", 0x800000},     {"Olive", 0x808000},
    {"Green", 0x008000},  {"Purple", 0x800080},     {"Teal", 0x008080},       {"Navy", 0x000080},
    {"Silver", 0xC0C0C0}, {"Pink", 0xFFC0CB},       {"Gold", 0xFFD700},       {"Brown", 0xA52A2A},
    {"Khaki", 0xF0E68C},  {"Lavender", 0xE6E6FA},   {"Light Blue", 0xADD8E6}, {"Light Green", 0x90EE90},
    {"Light Pink", 0xFFB6C1}, {"Beige", 0xD7CCA1},
};
}  // namespace color_names_detail

/// Nearest named colour (plain RGB distance). *exact is set when rgb is that colour exactly.
inline const char* nearestColorName(uint32_t rgb, bool* exact = nullptr) {
    using namespace color_names_detail;
    const char* best = kNamed[0].name;
    uint32_t bestDist = UINT32_MAX;
    for (const auto& c : kNamed) {
        const int dr = (int)((rgb >> 16) & 0xFF) - (int)((c.rgb >> 16) & 0xFF);
        const int dg = (int)((rgb >> 8) & 0xFF) - (int)((c.rgb >> 8) & 0xFF);
        const int db = (int)(rgb & 0xFF) - (int)(c.rgb & 0xFF);
        const uint32_t d = (uint32_t)(dr * dr + dg * dg + db * db);
        if (d < bestDist) { bestDist = d; best = c.name; }
    }
    if (exact) *exact = bestDist == 0;
    return best;
}

/// RGB of a DB colour string. The DB uses "#RRGGBB", and "#RRGGBBAA" for clear (AA=00) and
/// translucent (AA=80) filaments; the tag and the UI are RGB only, so the alpha byte is dropped.
inline uint32_t dbHexToRgb(const char* s) {
    if (!s) return 0;
    if (s[0] == '#') s++;
    char buf[7] = {0};
    for (int i = 0; i < 6 && s[i]; i++) buf[i] = s[i];
    return (uint32_t)strtoul(buf, nullptr, 16);
}

/// Alpha of a DB colour string: the AA of "#RRGGBBAA", or 0xFF for "#RRGGBB".
inline uint8_t dbHexAlpha(const char* s) {
    if (!s) return 0xFF;
    if (s[0] == '#') s++;
    if (std::strlen(s) < 8) return 0xFF;
    char buf[3] = {s[6], s[7], 0};
    return (uint8_t)strtoul(buf, nullptr, 16);
}

/// The DB's English name for one of a product's colours, or "" if it has none. When an opaque and
/// a clear entry share the RGB (CR-PETG White #FFFFFF / Transparent #FFFFFF00), the opaque one wins:
/// a tag can't tell them apart.
inline std::string productColorName(JsonArrayConst mix, uint32_t rgb) {
    if (mix.isNull()) return "";
    std::string translucent;
    bool haveTranslucent = false;
    for (JsonObjectConst m : mix) {
        const char* hex = m["hexValue"] | "";
        if (!hex[0] || dbHexToRgb(hex) != rgb) continue;
        const char* name = m["colorEn"] | "";
        if (dbHexAlpha(hex) == 0xFF) return name;
        if (!haveTranslucent) { translucent = name; haveTranslucent = true; }
    }
    return translucent;
}

/// The DB's English name for the entry whose hexValue is exactly `hex` (case-insensitive, alpha
/// included), or "" if none. Used when the DB colour string itself is known.
inline std::string productColorNameForHex(JsonArrayConst mix, const char* hex) {
    if (mix.isNull() || !hex) return "";
    if (hex[0] == '#') hex++;
    for (JsonObjectConst m : mix) {
        const char* h = m["hexValue"] | "";
        if (h[0] == '#') h++;
        if (!h[0] || std::strlen(h) != std::strlen(hex)) continue;
        bool same = true;
        for (size_t i = 0; h[i]; i++)
            if (std::toupper((unsigned char)h[i]) != std::toupper((unsigned char)hex[i])) { same = false; break; }
        if (same) return m["colorEn"] | "";
    }
    return "";
}

/// Two-line label: "Dark Green\n#212721", or "~Red\n#E00000" when only a nearest name is known.
inline void formatColorLabel(const char* productName, uint32_t rgb, char* out, size_t n) {
    if (productName && productName[0]) {
        std::snprintf(out, n, "%s\n#%06X", productName, (unsigned)(rgb & 0xFFFFFF));
        return;
    }
    bool exact = false;
    const char* name = nearestColorName(rgb, &exact);
    std::snprintf(out, n, "%s%s\n#%06X", exact ? "" : "~", name, (unsigned)(rgb & 0xFFFFFF));
}
