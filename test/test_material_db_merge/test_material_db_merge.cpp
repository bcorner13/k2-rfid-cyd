// Host tests for merging a printer DB download: pio test -e native -f test_material_db_merge
#include <unity.h>
#include <ArduinoJson.h>
#include <material_db_merge.h>

void setUp() {}
void tearDown() {}

// Current DB (the richer copy): 02001 with 3 named colours, 05001 that the printer doesn't list.
static const char* CURRENT = R"({"result":{"list":[
  {"base":{"id":"02001","name":"Hyper PLA-CF","colors":["#000000","#212721","#74654F"],
           "mixColorInfoList":[{"colorEn":"Black","hexValue":"#000000"},{"colorEn":"Dark Green","hexValue":"#212721"}]},
   "kvParam":{"nozzle_temperature":"220"}},
  {"base":{"id":"05001","name":"CR-Silk","colors":["#FFD700"]},"kvParam":{}},
  {"base":{"id":"05001","name":"CR-Silk @preset","colors":["#FFD700"]},"kvParam":{}}
]}})";

// Printer download: 02001 with one colour, no names, new temperature; plus a new 99001.
static const char* INCOMING = R"({"result":{"list":[
  {"base":{"id":"02001","name":"Hyper PLA-CF","colors":["#000000"],"mixColorInfoList":[]},
   "kvParam":{"nozzle_temperature":"225"}},
  {"base":{"id":"99001","name":"New Thing","colors":["#FFFFFF"]},"kvParam":{}}
]}})";

static JsonObject findId(JsonDocument& d, const char* id) {
    for (JsonObject e : d["result"]["list"].as<JsonArray>())
        if (strcmp(e["base"]["id"] | "", id) == 0) return e;
    return JsonObject();
}

void test_printer_values_win_but_colours_kept() {
    JsonDocument cur, inc;
    deserializeJson(cur, CURRENT);
    deserializeJson(inc, INCOMING);
    DbMergeStats st;
    TEST_ASSERT_TRUE(mergeMaterialDb(cur, inc, st));
    JsonObject e = findId(inc, "02001");
    TEST_ASSERT_EQUAL_STRING("225", e["kvParam"]["nozzle_temperature"]);       // printer's newer value
    TEST_ASSERT_EQUAL(3, e["base"]["colors"].size());                           // our 3 colours kept
    TEST_ASSERT_EQUAL_STRING("Dark Green", e["base"]["mixColorInfoList"][1]["colorEn"]);
    TEST_ASSERT_EQUAL(1u, st.updated);
    TEST_ASSERT_EQUAL(1u, st.coloursKept);
}

void test_new_printer_profiles_added() {
    JsonDocument cur, inc;
    deserializeJson(cur, CURRENT);
    deserializeJson(inc, INCOMING);
    DbMergeStats st;
    mergeMaterialDb(cur, inc, st);
    TEST_ASSERT_FALSE(findId(inc, "99001").isNull());
    TEST_ASSERT_EQUAL(1u, st.added);
}

// Profiles the printer's factory file lacks (CR-Silk, Ender-PLA...) must survive, presets too.
void test_local_only_profiles_kept_in_order() {
    JsonDocument cur, inc;
    deserializeJson(cur, CURRENT);
    deserializeJson(inc, INCOMING);
    DbMergeStats st;
    mergeMaterialDb(cur, inc, st);
    JsonArray list = inc["result"]["list"];
    TEST_ASSERT_EQUAL(4, list.size());
    TEST_ASSERT_EQUAL_STRING("CR-Silk", list[2]["base"]["name"]);
    TEST_ASSERT_EQUAL_STRING("CR-Silk @preset", list[3]["base"]["name"]);
    TEST_ASSERT_EQUAL(2u, st.keptLocalOnly);
}

void test_richer_printer_colours_win() {
    JsonDocument cur, inc;
    deserializeJson(cur, R"({"result":{"list":[{"base":{"id":"1","colors":["#000000"],
        "mixColorInfoList":[{"colorEn":"Black","hexValue":"#000000"}]}}]}})");
    deserializeJson(inc, R"({"result":{"list":[{"base":{"id":"1","colors":["#000000","#FFFFFF"],
        "mixColorInfoList":[{"colorEn":"Jet","hexValue":"#000000"},{"colorEn":"Snow","hexValue":"#FFFFFF"}]}}]}})");
    DbMergeStats st;
    mergeMaterialDb(cur, inc, st);
    JsonObject b = inc["result"]["list"][0]["base"];
    TEST_ASSERT_EQUAL(2, b["colors"].size());
    TEST_ASSERT_EQUAL_STRING("Jet", b["mixColorInfoList"][0]["colorEn"]);
    TEST_ASSERT_EQUAL(0u, st.coloursKept);
}

// Hyper PC: board and printer both list one colour, so "longer wins" never added Transparent.
// Colours are a union: anything either side lists survives; same hex (any case) is one colour.
void test_colours_are_a_union() {
    JsonDocument cur, inc;
    deserializeJson(cur, R"({"result":{"list":[{"base":{"id":"07002","colors":["#000000"],
        "mixColorInfoList":[{"colorEn":"Black","hexValue":"#000000"}]}}]}})");
    deserializeJson(inc, R"({"result":{"list":[{"base":{"id":"07002","colors":["#000000","#ffffff00"],
        "mixColorInfoList":[{"colorEn":"Transparent","hexValue":"#FFFFFF00"}]}}]}})");
    DbMergeStats st;
    mergeMaterialDb(cur, inc, st);
    JsonObject b = inc["result"]["list"][0]["base"];
    TEST_ASSERT_EQUAL(2, b["colors"].size());
    TEST_ASSERT_EQUAL(2, b["mixColorInfoList"].size());
    TEST_ASSERT_EQUAL_STRING("Black", b["mixColorInfoList"][1]["colorEn"]);   // name from current
}

void test_union_fills_missing_name_only() {
    JsonDocument cur, inc;
    deserializeJson(cur, R"({"result":{"list":[{"base":{"id":"1","colors":["#111111"],
        "mixColorInfoList":[{"colorEn":"Coal","hexValue":"#111111"}]}}]}})");
    deserializeJson(inc, R"({"result":{"list":[{"base":{"id":"1","colors":["#111111"],
        "mixColorInfoList":[{"colorEn":"","hexValue":"#111111"}]}}]}})");
    DbMergeStats st;
    mergeMaterialDb(cur, inc, st);
    JsonObject b = inc["result"]["list"][0]["base"];
    TEST_ASSERT_EQUAL(1, b["colors"].size());
    TEST_ASSERT_EQUAL(1, b["mixColorInfoList"].size());
    TEST_ASSERT_EQUAL_STRING("Coal", b["mixColorInfoList"][0]["colorEn"]);
}

// Firmware-bundled product colours (from data/material_database.json) are a third source.
static const char* BUNDLED = R"({"07002":{"colors":["#000000","#FFFFFF00"],
    "mixColorInfoList":[{"colorEn":"Black","hexValue":"#000000"},{"colorEn":"Transparent","hexValue":"#FFFFFF00"}]},
  "05001":{"colors":["#C0C0C0"],"mixColorInfoList":[{"colorEn":"Silver","hexValue":"#C0C0C0"}]}})";

void test_bundled_colours_added_everywhere() {
    JsonDocument cur, inc, bun;
    deserializeJson(cur, R"({"result":{"list":[
        {"base":{"id":"05001","name":"CR-Silk","colors":["#FFD700"]}}]}})");          // local only
    deserializeJson(inc, R"({"result":{"list":[{"base":{"id":"07002","colors":["#000000"]}}]}})");
    deserializeJson(bun, BUNDLED);
    DbMergeStats st;
    mergeMaterialDb(cur, inc, st, &bun);
    JsonObject pc = findId(inc, "07002")["base"];
    TEST_ASSERT_EQUAL(2, pc["colors"].size());
    TEST_ASSERT_EQUAL_STRING("#FFFFFF00", pc["colors"][1]);
    TEST_ASSERT_EQUAL_STRING("Transparent", pc["mixColorInfoList"][1]["colorEn"]);
    JsonObject silk = findId(inc, "05001")["base"];
    TEST_ASSERT_EQUAL(2, silk["colors"].size());
    TEST_ASSERT_EQUAL(2u, st.bundledColoursAdded);   // 07002 Transparent + 05001 Silver
}

void test_bundled_merge_is_idempotent() {
    JsonDocument cur, inc, bun;
    deserializeJson(inc, R"({"result":{"list":[{"base":{"id":"07002","colors":["#000000","#ffffff00"]}}]}})");
    deserializeJson(bun, BUNDLED);
    DbMergeStats st;
    mergeMaterialDb(cur, inc, st, &bun);
    TEST_ASSERT_EQUAL(2, inc["result"]["list"][0]["base"]["colors"].size());
    TEST_ASSERT_EQUAL(0u, st.bundledColoursAdded);
}

void test_no_current_db_takes_download_as_is() {
    JsonDocument cur, inc;
    deserializeJson(inc, INCOMING);
    DbMergeStats st;
    TEST_ASSERT_TRUE(mergeMaterialDb(cur, inc, st));
    TEST_ASSERT_EQUAL(2, inc["result"]["list"].size());
    TEST_ASSERT_EQUAL(2u, st.added);
}

void test_bad_download_rejected() {
    JsonDocument cur, inc;
    deserializeJson(cur, CURRENT);
    deserializeJson(inc, R"({"result":{}})");
    DbMergeStats st;
    TEST_ASSERT_FALSE(mergeMaterialDb(cur, inc, st));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_printer_values_win_but_colours_kept);
    RUN_TEST(test_new_printer_profiles_added);
    RUN_TEST(test_local_only_profiles_kept_in_order);
    RUN_TEST(test_richer_printer_colours_win);
    RUN_TEST(test_colours_are_a_union);
    RUN_TEST(test_union_fills_missing_name_only);
    RUN_TEST(test_bundled_colours_added_everywhere);
    RUN_TEST(test_bundled_merge_is_idempotent);
    RUN_TEST(test_no_current_db_takes_download_as_is);
    RUN_TEST(test_bad_download_rejected);
    return UNITY_END();
}
