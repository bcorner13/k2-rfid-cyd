#pragma once
/**
 * @file material_db_merge.h
 * @brief Merge a printer material_database.json download into the current one. Pure; host-tested.
 *
 * The printer's factory file (/downloads/defData/material/F008_...) is poorer than the copy on
 * the device: 75 vs 118 profiles on 2026-11-09, one colour per product and no colour names.
 * Replacing the file outright lost CR-Silk / Ender-PLA and every product colour. So:
 *  - a profile in both: the printer's entry wins (temperatures etc.), except base.colors and
 *    base.mixColorInfoList, which are kept from the current DB when it lists more colours;
 *  - a profile only on the printer: added;
 *  - a profile only in the current DB: kept, appended in its original order (presets included).
 * The result is built in `incoming`. Returns false (and leaves the caller to keep the current
 * file) when `incoming` has no result.list. An empty `current` means "take the download as is".
 */
#include <cstring>
#include <ArduinoJson.h>

struct DbMergeStats {
    unsigned updated = 0;        // in both
    unsigned added = 0;          // printer only
    unsigned keptLocalOnly = 0;  // current only
    unsigned coloursKept = 0;    // in both, colours taken from the current DB
};

inline bool mergeMaterialDb(JsonDocument& current, JsonDocument& incoming, DbMergeStats& st) {
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
            nb["colors"] = ob["colors"];
            if (!ob["mixColorInfoList"].isNull()) nb["mixColorInfoList"] = ob["mixColorInfoList"];
            st.coloursKept++;
        }
    }

    if (cur.isNull()) return true;
    for (JsonObject old : cur) {
        const char* id = old["base"]["id"] | "";
        bool onPrinter = false;
        for (size_t i = 0; i < incomingCount && !onPrinter; i++)
            onPrinter = strcmp(in[i]["base"]["id"] | "", id) == 0;
        if (onPrinter) continue;
        in.add(old);
        st.keptLocalOnly++;
    }
    return true;
}
