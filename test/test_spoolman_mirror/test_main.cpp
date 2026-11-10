// Host tests for parsing Spoolman filament/spool pages into local-mirror rows: pio test -e native
// Fixtures captured from a real Spoolman v0.27.1 (Docker, 2026-11-09). Null fields are omitted.
#include <unity.h>
#include <cstring>
#include <string>
#include "spoolman_mirror_parse.h"

using namespace spoolman;

void setUp() {}
void tearDown() {}

static const char* FILAMENT_SINGLE = R"([{"id":1,"registered":"2026-11-10T00:03:14Z","name":"Ender-PLA",
 "vendor":{"id":1,"registered":"2026-11-10T00:03:14Z","name":"Creality","extra":{}},"material":"PLA",
 "density":1.24,"diameter":1.75,"weight":1000.0,"color_hex":"e53935","settings_extruder_temp":210,
 "settings_bed_temp":60,"extra":{"cfs_material_id":"\"08001\""},"tags":[]}])";

static const char* FILAMENT_MULTI = R"([{"id":2,"registered":"2026-11-10T00:03:14Z","name":"Silk Dual",
 "vendor":{"id":1,"name":"Creality","extra":{}},"material":"PLA","density":1.24,"diameter":1.75,
 "multi_color_hexes":"C62828,c9a227","multi_color_direction":"coaxial","extra":{},"tags":[]}])";

static const char* SPOOL_PAGE = R"([{"id":1,"registered":"2026-11-10T00:03:25Z","filament":{"id":1,
 "name":"Ender-PLA","vendor":{"id":1,"name":"Creality","extra":{}},"material":"PLA","color_hex":"E53935",
 "extra":{"cfs_material_id":"\"08001\""},"tags":[]},"remaining_weight":742.5,"initial_weight":1000.0,
 "used_weight":257.5,"location":"K2 CFS","archived":false,"extra":{"k2rfid_change":"\"a1b2c3d4-7\""},
 "tags":[{"uid":"04:a2:b3:c4","format":"creality","added":"2026-11-10T00:03:25Z"}]},
 {"id":2,"filament":{"id":2,"name":"Silk Dual","vendor":{"id":1,"name":"Creality"}},"initial_weight":500.0,
 "archived":true,"extra":{},"tags":[]}])";

// ---- filaments ----

void test_parse_filament_single_color() {
    std::vector<MirrorFilament> out; size_t count = 0;
    TEST_ASSERT_EQUAL(static_cast<int>(PageResult::Ok),
                      static_cast<int>(parseFilamentPage(FILAMENT_SINGLE, strlen(FILAMENT_SINGLE), out, count)));
    TEST_ASSERT_EQUAL_UINT(1, count);
    TEST_ASSERT_EQUAL_UINT(1, out.size());
    const MirrorFilament& f = out[0];
    TEST_ASSERT_EQUAL(1, f.id);
    TEST_ASSERT_EQUAL(1, f.vendorId);
    TEST_ASSERT_EQUAL_STRING("Creality", f.vendor.c_str());
    TEST_ASSERT_EQUAL_STRING("Ender-PLA", f.name.c_str());
    TEST_ASSERT_EQUAL_STRING("PLA", f.material.c_str());
    TEST_ASSERT_EQUAL_STRING("E53935", f.colorHex.c_str());
    TEST_ASSERT_EQUAL_UINT(0, f.multiColors.size());
    TEST_ASSERT_EQUAL_STRING("", f.direction.c_str());
    TEST_ASSERT_EQUAL_FLOAT(1000.0f, f.weightG);
    TEST_ASSERT_EQUAL(210, f.nozzleC);
    TEST_ASSERT_EQUAL(60, f.bedC);
}

void test_parse_filament_multi_coaxial() {
    std::vector<MirrorFilament> out; size_t count = 0;
    parseFilamentPage(FILAMENT_MULTI, strlen(FILAMENT_MULTI), out, count);
    TEST_ASSERT_EQUAL_UINT(1, out.size());
    TEST_ASSERT_EQUAL_UINT(2, out[0].multiColors.size());
    TEST_ASSERT_EQUAL_STRING("C62828", out[0].multiColors[0].c_str());
    TEST_ASSERT_EQUAL_STRING("C9A227", out[0].multiColors[1].c_str());
    TEST_ASSERT_EQUAL_STRING("coaxial", out[0].direction.c_str());
    TEST_ASSERT_EQUAL_STRING("", out[0].colorHex.c_str());
}

void test_parse_filament_cfs_extra() {
    std::vector<MirrorFilament> out; size_t count = 0;
    parseFilamentPage(FILAMENT_SINGLE, strlen(FILAMENT_SINGLE), out, count);
    parseFilamentPage(FILAMENT_MULTI, strlen(FILAMENT_MULTI), out, count);   // appends
    TEST_ASSERT_EQUAL_UINT(2, out.size());
    TEST_ASSERT_EQUAL_STRING("08001", out[0].cfsId.c_str());
    TEST_ASSERT_EQUAL_STRING("", out[1].cfsId.c_str());
}

// ---- spools ----

void test_parse_spool_tags_and_archived() {
    std::vector<MirrorSpool> out; size_t count = 0;
    TEST_ASSERT_EQUAL(static_cast<int>(PageResult::Ok),
                      static_cast<int>(parseSpoolPage(SPOOL_PAGE, strlen(SPOOL_PAGE), out, count)));
    TEST_ASSERT_EQUAL_UINT(2, out.size());
    const MirrorSpool& s = out[0];
    TEST_ASSERT_EQUAL(1, s.id);
    TEST_ASSERT_EQUAL(1, s.filamentId);
    TEST_ASSERT_EQUAL_FLOAT(1000.0f, s.initialG);
    TEST_ASSERT_TRUE(s.hasRemaining);
    TEST_ASSERT_EQUAL_FLOAT(742.5f, s.remainingG);
    TEST_ASSERT_EQUAL_STRING("K2 CFS", s.location.c_str());
    TEST_ASSERT_FALSE(s.archived);
    TEST_ASSERT_EQUAL_STRING("a1b2c3d4-7", s.changeKey.c_str());
    TEST_ASSERT_EQUAL_UINT(1, s.uids.size());
    TEST_ASSERT_EQUAL_STRING("04A2B3C4", s.uids[0].c_str());
    TEST_ASSERT_TRUE(out[1].archived);
    TEST_ASSERT_EQUAL(2, out[1].filamentId);
}

void test_parse_spool_no_remaining() {
    std::vector<MirrorSpool> out; size_t count = 0;
    parseSpoolPage(SPOOL_PAGE, strlen(SPOOL_PAGE), out, count);
    TEST_ASSERT_FALSE(out[1].hasRemaining);
    TEST_ASSERT_EQUAL_UINT(0, out[1].uids.size());
    TEST_ASSERT_EQUAL_STRING("", out[1].changeKey.c_str());
}

void test_parse_spool_page_malformed() {
    std::vector<MirrorSpool> out; size_t count = 99;
    out.push_back(MirrorSpool{});
    out[0].id = 42;
    const char* html = "<html><body>Captive portal</body></html>";
    TEST_ASSERT_EQUAL(static_cast<int>(PageResult::Malformed),
                      static_cast<int>(parseSpoolPage(html, strlen(html), out, count)));
    const char* truncated = R"([{"id":1,"filament":{"id":1},"initial_weight":1000.0,"tags":[{"uid":"04)";
    TEST_ASSERT_EQUAL(static_cast<int>(PageResult::Malformed),
                      static_cast<int>(parseSpoolPage(truncated, strlen(truncated), out, count)));
    const char* object = R"({"detail":"Not found"})";
    TEST_ASSERT_EQUAL(static_cast<int>(PageResult::Malformed),
                      static_cast<int>(parseSpoolPage(object, strlen(object), out, count)));
    TEST_ASSERT_EQUAL_UINT(1, out.size());   // unchanged
    TEST_ASSERT_EQUAL(42, out[0].id);

    std::vector<MirrorFilament> fout;
    TEST_ASSERT_EQUAL(static_cast<int>(PageResult::Malformed),
                      static_cast<int>(parseFilamentPage(html, strlen(html), fout, count)));
    TEST_ASSERT_EQUAL_UINT(0, fout.size());
}

void test_parse_page_count() {
    const char* three = R"([{"id":1,"filament":{"id":1}},{"id":2,"filament":{"id":1}},{"id":3,"filament":{"id":1}}])";
    std::vector<MirrorSpool> out; size_t count = 0;
    parseSpoolPage(three, strlen(three), out, count);
    TEST_ASSERT_EQUAL_UINT(3, count);
    const char* empty = "[]";
    TEST_ASSERT_EQUAL(static_cast<int>(PageResult::Ok),
                      static_cast<int>(parseSpoolPage(empty, strlen(empty), out, count)));
    TEST_ASSERT_EQUAL_UINT(0, count);
    TEST_ASSERT_EQUAL_UINT(3, out.size());
}

// ---- helpers ----

void test_normalize_hex() {
    TEST_ASSERT_EQUAL_STRING("E53935", normalizeHex("#e53935").c_str());
    TEST_ASSERT_EQUAL_STRING("E53935", normalizeHex("e53935ff").c_str());   // RRGGBBAA -> RRGGBB
    TEST_ASSERT_EQUAL_STRING("", normalizeHex("e539").c_str());
    TEST_ASSERT_EQUAL_STRING("", normalizeHex("zz3935").c_str());
    TEST_ASSERT_EQUAL_STRING("", normalizeHex("").c_str());
}

void test_decode_extra_string() {
    TEST_ASSERT_EQUAL_STRING("08001", decodeExtraString("\"08001\"").c_str());
    TEST_ASSERT_EQUAL_STRING("", decodeExtraString("123").c_str());
    TEST_ASSERT_EQUAL_STRING("", decodeExtraString("").c_str());
    TEST_ASSERT_EQUAL_STRING("", decodeExtraString(nullptr).c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_filament_single_color);
    RUN_TEST(test_parse_filament_multi_coaxial);
    RUN_TEST(test_parse_filament_cfs_extra);
    RUN_TEST(test_parse_spool_tags_and_archived);
    RUN_TEST(test_parse_spool_no_remaining);
    RUN_TEST(test_parse_spool_page_malformed);
    RUN_TEST(test_parse_page_count);
    RUN_TEST(test_normalize_hex);
    RUN_TEST(test_decode_extra_string);
    return UNITY_END();
}
