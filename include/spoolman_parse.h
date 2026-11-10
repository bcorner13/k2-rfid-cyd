#pragma once
/**
 * @file spoolman_parse.h
 * @brief Pure Spoolman logic: UID normalization, lookup URL, response parsing, display text.
 *
 * No Arduino or FreeRTOS dependencies, so it runs under `pio test -e native`.
 * Response shape: Spoolman v0.27.1 `GET /api/v1/spool?tag=<uid>`. Nullable fields are omitted.
 */
#include <ArduinoJson.h>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace spoolman {

constexpr size_t UID_BUF = 21;       // 10-byte UID as hex + NUL (MIFARE Classic uses 4)
constexpr size_t URL_BUF = 224;
constexpr size_t BASE_URL_BUF = 128;
constexpr size_t SUMMARY_BUF = 160;

enum class UrlResult : uint8_t { Ok, Disabled, HttpsUnsupported, TooLong };
enum class ParseResult : uint8_t { Found, NotFound, Mismatch, Malformed };

// Fixed-size and trivially copyable: it crosses a FreeRTOS queue.
struct SpoolInfo {
    int id = 0;
    char name[48] = "";
    char vendor[32] = "";
    char material[16] = "";
    char location[32] = "";
    bool hasRemaining = false;
    float remainingG = 0.0f;
    bool archived = false;
};

// Uppercase hex with separators (":", "-", spaces) removed. False on non-hex, empty, or overflow.
inline bool normalizeUid(const char* in, char* out, size_t outSize) {
    size_t n = 0;
    for (; *in; ++in) {
        const unsigned char c = static_cast<unsigned char>(*in);
        if (c == ':' || c == '-' || c == ' ') continue;
        if (!std::isxdigit(c) || n + 1 >= outSize) return false;
        out[n++] = static_cast<char>(std::toupper(c));
    }
    if (n == 0) return false;
    out[n] = '\0';
    return true;
}

// `<base>/api/v1<pathAndQuery>` from the configured base URL ("http://" added when missing,
// surrounding whitespace and trailing slashes dropped). https is reported, never downgraded.
inline UrlResult buildApiUrl(const char* baseUrl, const char* pathAndQuery, char* out, size_t outSize) {
    while (*baseUrl && std::isspace(static_cast<unsigned char>(*baseUrl))) ++baseUrl;
    size_t len = std::strlen(baseUrl);
    while (len > 0 && (std::isspace(static_cast<unsigned char>(baseUrl[len - 1])) || baseUrl[len - 1] == '/')) --len;
    if (len == 0) return UrlResult::Disabled;

    bool hasHttp = len >= 7, isHttps = len >= 8;
    const char* httpScheme = "http://";
    const char* httpsScheme = "https://";
    for (size_t i = 0; hasHttp && i < 7; ++i) hasHttp = std::tolower(static_cast<unsigned char>(baseUrl[i])) == httpScheme[i];
    for (size_t i = 0; isHttps && i < 8; ++i) isHttps = std::tolower(static_cast<unsigned char>(baseUrl[i])) == httpsScheme[i];
    if (isHttps) return UrlResult::HttpsUnsupported;  // needs TLS client + cert handling; never downgrade silently

    const int written = std::snprintf(out, outSize, "%s%.*s/api/v1%s",
                                      hasHttp ? "" : httpScheme, static_cast<int>(len), baseUrl, pathAndQuery);
    if (written < 0 || static_cast<size_t>(written) >= outSize) return UrlResult::TooLong;
    return UrlResult::Ok;
}

// `normalizedUid` must already be hex, so it needs no URL encoding. Archived spools are included:
// otherwise a tag on an archived spool would look unknown, and linking it later would hit a 409.
inline UrlResult buildLookupUrl(const char* baseUrl, const char* normalizedUid, char* out, size_t outSize) {
    char path[64];
    std::snprintf(path, sizeof(path), "/spool?tag=%s&allow_archived=true", normalizedUid);
    return buildApiUrl(baseUrl, path, out, outSize);
}

namespace detail {
inline void copyField(char* dst, size_t dstSize, const char* src) {
    std::snprintf(dst, dstSize, "%s", src ? src : "");
}
}  // namespace detail

// Finds the spool carrying `normalizedUid`. Spools without that tag are never returned: if a server
// ignored ?tag= it would list every spool, and the first one would be wrong data shown as fact.
inline ParseResult parseLookup(const char* json, size_t len, const char* normalizedUid, SpoolInfo& out) {
    JsonDocument filter;
    JsonObject f = filter[0].to<JsonObject>();
    f["id"] = true;
    f["remaining_weight"] = true;
    f["location"] = true;
    f["archived"] = true;
    f["tags"][0]["uid"] = true;
    f["filament"]["name"] = true;
    f["filament"]["material"] = true;
    f["filament"]["vendor"]["name"] = true;

    JsonDocument doc;
    if (deserializeJson(doc, json, len, DeserializationOption::Filter(filter))) return ParseResult::Malformed;
    if (!doc.is<JsonArray>()) return ParseResult::Malformed;

    JsonArray spools = doc.as<JsonArray>();
    if (spools.size() == 0) return ParseResult::NotFound;

    for (JsonObject spool : spools) {
        for (JsonObject tag : spool["tags"].as<JsonArray>()) {
            char tagUid[UID_BUF];
            const char* raw = tag["uid"] | "";
            if (!normalizeUid(raw, tagUid, sizeof(tagUid)) || std::strcmp(tagUid, normalizedUid) != 0) continue;

            out = SpoolInfo{};
            out.id = spool["id"] | 0;
            detail::copyField(out.name, sizeof(out.name), spool["filament"]["name"] | "");
            detail::copyField(out.material, sizeof(out.material), spool["filament"]["material"] | "");
            detail::copyField(out.vendor, sizeof(out.vendor), spool["filament"]["vendor"]["name"] | "");
            detail::copyField(out.location, sizeof(out.location), spool["location"] | "");
            out.archived = spool["archived"] | false;
            out.hasRemaining = spool["remaining_weight"].is<float>();
            if (out.hasRemaining) out.remainingG = spool["remaining_weight"].as<float>();
            return ParseResult::Found;
        }
    }
    return ParseResult::Mismatch;
}

// Two lines for the dashboard, e.g. "Spoolman #12: eSUN PETG+\n742 g left - K2 CFS".
inline void formatSummary(const SpoolInfo& s, char* out, size_t outSize) {
    const char* filament = s.name[0] ? s.name : s.material;
    char remaining[24];
    if (s.hasRemaining) std::snprintf(remaining, sizeof(remaining), "%.0f g left", s.remainingG);
    else std::snprintf(remaining, sizeof(remaining), "Remaining unknown");

    std::snprintf(out, outSize, "Spoolman #%d%s: %s%s%s\n%s%s%s", s.id, s.archived ? " (archived)" : "",
                  s.vendor, s.vendor[0] ? " " : "", filament, remaining,
                  s.location[0] ? " - " : "", s.location);
}

}  // namespace spoolman
