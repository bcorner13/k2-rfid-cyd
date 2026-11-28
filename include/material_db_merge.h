#pragma once
/**
 * @file material_db_merge.h
 * @brief Merge a printer material_database.json download into the current one. Pure; host-tested.
 *
 * The printer's factory file (/downloads/defData/material/F008_...) is poorer than the copy on
 * the device: 75 vs 118 profiles on 2026-11-09, one colour per product and no colour names.
 * Replacing the file outright lost CR-Silk / Ender-PLA and every product colour. So:
 *  - a profile in both: the printer's entry wins (temperatures etc.), except the colours: base.colors
 *    and base.mixColorInfoList are a union of both sides (the longer list keeps its order, the
 *    other side's missing colours are appended). Colours match by hex, alpha included, any case;
 *  - a profile only on the printer: added;
 *  - a profile only in the current DB: kept, appended in its original order (presets included);
 *  - `bundled` (optional, {"<id>": {colors, mixColorInfoList}}, generated from
 *    data/material_database.json into bundled_colors.h) is unioned into every entry with that id.
 *    The board's DB never had Hyper PC's Transparent, and the printer lists #000000 only, so
 *    without this no merge could ever add it.
 * The result is built in `incoming`. Returns false (and leaves the caller to keep the current
 * file) when `incoming` has no result.list. An empty `current` means "take the download as is".
 */
#include <cctype>
#include <cstring>
#include <ArduinoJson.h>

struct DbMergeStats {
    unsigned updated = 0;              // in both
    unsigned added = 0;                // printer only
    unsigned keptLocalOnly = 0;        // current only
    unsigned coloursKept = 0;          // in both, the current DB's list was the longer one
    unsigned bundledColoursAdded = 0;  // colours added from the firmware's bundled list
};

namespace db_merge_detail {
inline bool sameHex(const char* a, const char* b) {
    if (!a || !b) return false;
    if (*a == '#') a++;
    if (*b == '#') b++;
    for (; *a && *b; a++, b++)
        if (std::toupper((unsigned char)*a) != std::toupper((unsigned char)*b)) return false;
    return *a == *b;
}

/// Union src's colours and colour names into dst (both "base"-shaped). Returns colours added.
inline unsigned unionColours(JsonObject dst, JsonObjectConst src) {
    if (src.isNull()) return 0;
    unsigned added = 0;
    JsonArrayConst sc = src["colors"];
    if (!sc.isNull() && sc.size() > 0) {
        JsonArray dc = dst["colors"].is<JsonArray>() ? dst["colors"].as<JsonArray>()
                                                     : dst["colors"].to<JsonArray>();
        for (JsonVariantConst c : sc) {
            const char* h = c | "";
            if (!h[0]) continue;
            bool have = false;
            for (JsonVariantConst d : dc) if (sameHex(d | "", h)) { have = true; break; }
            if (!have) { dc.add(c); added++; }
        }
    }
    JsonArrayConst sm = src["mixColorInfoList"];
    if (!sm.isNull() && sm.size() > 0) {
        JsonArray dm = dst["mixColorInfoList"].is<JsonArray>() ? dst["mixColorInfoList"].as<JsonArray>()
                                                               : dst["mixColorInfoList"].to<JsonArray>();
        for (JsonObjectConst m : sm) {
            const char* h = m["hexValue"] | "";
            if (!h[0]) continue;
            JsonObject match;
            for (JsonObject d : dm) if (sameHex(d["hexValue"] | "", h)) { match = d; break; }
            if (match.isNull()) { dm.add(m); continue; }
            const char* have = match["colorEn"] | "";
            const char* name = m["colorEn"] | "";
            if (!have[0] && name[0]) match["colorEn"] = m["colorEn"];   // fill a blank name only
        }
    }
    return added;
}
}  // namespace db_merge_detail

inline bool mergeMaterialDb(JsonDocument& current, JsonDocument& incoming, DbMergeStats& st,
                            const JsonDocument* bundled = nullptr) {
    using namespace db_merge_detail;
    st = DbMergeStats{};
    JsonArray in = incoming["result"]["list"].as<JsonArray>();
    if (in.isNull()) return false;
    JsonArray cur = current["result"]["list"].as<JsonArray>();

    auto findIn = [](JsonArray list, const char* id) -> JsonObject {
        if (list.isNull()) return JsonObject();
        for (JsonObject e : list)
            if (strcmp(e["base"]["id"] | "", id) == 0) return e;
        return JsonObject();
    };

    const size_t incomingCount = in.size();
    for (size_t i = 0; i < incomingCount; i++) {
        JsonObject e = in[i];
        const char* id = e["base"]["id"] | "";
        JsonObject old = findIn(cur, id);
        if (old.isNull()) { st.added++; continue; }
        st.updated++;
        JsonObject nb = e["base"], ob = old["base"];
        if (ob["colors"].size() > nb["colors"].size()) {
            // Ours is longer: it keeps its order, the printer's extra colours go after it.
            JsonDocument printerColours;
            printerColours["colors"] = nb["colors"];
            printerColours["mixColorInfoList"] = nb["mixColorInfoList"];
            nb["colors"] = ob["colors"];
            nb.remove("mixColorInfoList");
            if (!ob["mixColorInfoList"].isNull()) nb["mixColorInfoList"] = ob["mixColorInfoList"];
            unionColours(nb, printerColours.as<JsonObjectConst>());
            st.coloursKept++;
        } else {
            unionColours(nb, ob);
        }
    }

    if (!cur.isNull()) {
        for (JsonObject old : cur) {
            const char* id = old["base"]["id"] | "";
            bool onPrinter = false;
            for (size_t i = 0; i < incomingCount && !onPrinter; i++)
                onPrinter = strcmp(in[i]["base"]["id"] | "", id) == 0;
            if (onPrinter) continue;
            in.add(old);
            st.keptLocalOnly++;
        }
    }

    if (bundled) {
        JsonObjectConst bun = bundled->as<JsonObjectConst>();
        if (!bun.isNull()) {
            for (JsonObject e : in) {
                JsonObject b = e["base"];
                const char* id = b["id"] | "";
                if (!id[0]) continue;
                st.bundledColoursAdded += unionColours(b, bun[id]);
            }
        }
    }
    return true;
}
