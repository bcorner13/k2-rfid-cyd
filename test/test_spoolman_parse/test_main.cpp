// Host tests for Spoolman URL building and response parsing: pio test -e native
// Fixtures follow the v0.27.1 Spool model: null fields are omitted (exclude_none=true).
#include <unity.h>
#include <cstring>
#include <string>
#include "spoolman_parse.h"

using namespace spoolman;

void setUp() {}
void tearDown() {}

static const char* FOUND_JSON = R"([{
  "id": 12,
  "registered": "2026-11-05T12:00:00Z",
  "filament": {
    "id": 3, "name": "PETG+", "material": "PETG",
    "vendor": {"id": 1, "name": "eSUN"},
    "extra": {"ignored": "\"x\""}
  },
  "remaining_weight": 742.4,
  "used_weight": 257.6,
  "location": "K2 CFS",
  "archived": false,
  "extra": {},
  "tags": [{"uid": "04A2B3C4", "format": "creality"}]
}])";

// ---- normalizeUid ----

void test_normalize_strips_separators_and_uppercases() {
    char out[UID_BUF];
    TEST_ASSERT_TRUE(normalizeUid("04:a2:b3:c4", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("04A2B3C4", out);
}

void test_normalize_rejects_non_hex_empty_and_overflow() {
    char out[UID_BUF];
    TEST_ASSERT_FALSE(normalizeUid("04:ZZ", out, sizeof(out)));
    TEST_ASSERT_FALSE(normalizeUid("", out, sizeof(out)));
    char tiny[4];
    TEST_ASSERT_FALSE(normalizeUid("04A2B3C4", tiny, sizeof(tiny)));
}

// ---- buildLookupUrl ----

void test_url_plain() {
    char url[URL_BUF];
    TEST_ASSERT_EQUAL(UrlResult::Ok, buildLookupUrl("http://spoolman.lan:7912", "04A2B3C4", url, sizeof(url)));
    TEST_ASSERT_EQUAL_STRING("http://spoolman.lan:7912/api/v1/spool?tag=04A2B3C4&allow_archived=true", url);
}

void test_url_trailing_slash_and_whitespace() {
    char url[URL_BUF];
    TEST_ASSERT_EQUAL(UrlResult::Ok, buildLookupUrl("  http://192.0.2.10:7912/ ", "04A2B3C4", url, sizeof(url)));
    TEST_ASSERT_EQUAL_STRING("http://192.0.2.10:7912/api/v1/spool?tag=04A2B3C4&allow_archived=true", url);
}

void test_url_scheme_is_added_when_missing() {
    char url[URL_BUF];
    TEST_ASSERT_EQUAL(UrlResult::Ok, buildLookupUrl("spoolman.lan:7912", "04A2B3C4", url, sizeof(url)));
    TEST_ASSERT_EQUAL_STRING("http://spoolman.lan:7912/api/v1/spool?tag=04A2B3C4&allow_archived=true", url);
}

void test_url_https_is_reported_not_silently_downgraded() {
    char url[URL_BUF];
    TEST_ASSERT_EQUAL(UrlResult::HttpsUnsupported, buildLookupUrl("https://spoolman.lan", "04A2B3C4", url, sizeof(url)));
    TEST_ASSERT_EQUAL(UrlResult::HttpsUnsupported, buildLookupUrl("HTTPS://spoolman.lan", "04A2B3C4", url, sizeof(url)));
}

void test_url_empty_means_disabled() {
    char url[URL_BUF];
    TEST_ASSERT_EQUAL(UrlResult::Disabled, buildLookupUrl("", "04A2B3C4", url, sizeof(url)));
    TEST_ASSERT_EQUAL(UrlResult::Disabled, buildLookupUrl("   ", "04A2B3C4", url, sizeof(url)));
}

void test_url_too_long_fails() {
    char url[24];
    TEST_ASSERT_EQUAL(UrlResult::TooLong, buildLookupUrl("http://spoolman.lan:7912", "04A2B3C4", url, sizeof(url)));
}

void test_api_url_any_path() {
    char url[URL_BUF];
    TEST_ASSERT_EQUAL(UrlResult::Ok, buildApiUrl(" 192.0.2.20:7912/ ", "/filament?limit=50&offset=100", url, sizeof(url)));
    TEST_ASSERT_EQUAL_STRING("http://192.0.2.20:7912/api/v1/filament?limit=50&offset=100", url);
    TEST_ASSERT_EQUAL(UrlResult::Disabled, buildApiUrl("  ", "/spool", url, sizeof(url)));
    TEST_ASSERT_EQUAL(UrlResult::HttpsUnsupported, buildApiUrl("https://x", "/spool", url, sizeof(url)));
    char small[20];
    TEST_ASSERT_EQUAL(UrlResult::TooLong, buildApiUrl("http://spoolman.lan:7912", "/spool", small, sizeof(small)));
}

// ---- parseLookup ----

void test_parse_found() {
    SpoolInfo s;
    TEST_ASSERT_EQUAL(ParseResult::Found, parseLookup(FOUND_JSON, strlen(FOUND_JSON), "04A2B3C4", s));
    TEST_ASSERT_EQUAL(12, s.id);
    TEST_ASSERT_EQUAL_STRING("PETG+", s.name);
    TEST_ASSERT_EQUAL_STRING("eSUN", s.vendor);
    TEST_ASSERT_EQUAL_STRING("PETG", s.material);
    TEST_ASSERT_EQUAL_STRING("K2 CFS", s.location);
    TEST_ASSERT_TRUE(s.hasRemaining);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 742.4f, s.remainingG);
    TEST_ASSERT_FALSE(s.archived);
}

void test_parse_empty_list_is_not_found() {
    SpoolInfo s;
    TEST_ASSERT_EQUAL(ParseResult::NotFound, parseLookup("[]", 2, "04A2B3C4", s));
}

// If a server ignored ?tag= it would return every spool. Never show one that isn't linked to this tag.
void test_parse_spool_without_this_tag_is_a_mismatch() {
    SpoolInfo s;
    TEST_ASSERT_EQUAL(ParseResult::Mismatch, parseLookup(FOUND_JSON, strlen(FOUND_JSON), "DEADBEEF", s));
}

void test_parse_picks_the_spool_carrying_the_tag() {
    const char* json = R"([
      {"id": 1, "filament": {"name": "Wrong"}, "archived": false, "tags": [{"uid": "11111111"}]},
      {"id": 2, "filament": {"name": "Right"}, "archived": false, "tags": [{"uid": "04A2B3C4"}]}
    ])";
    SpoolInfo s;
    TEST_ASSERT_EQUAL(ParseResult::Found, parseLookup(json, strlen(json), "04A2B3C4", s));
    TEST_ASSERT_EQUAL(2, s.id);
    TEST_ASSERT_EQUAL_STRING("Right", s.name);
}

void test_parse_omitted_optional_fields() {
    const char* json = R"([{"id": 7, "filament": {"material": "PLA"}, "archived": true, "tags": [{"uid": "04A2B3C4"}]}])";
    SpoolInfo s;
    TEST_ASSERT_EQUAL(ParseResult::Found, parseLookup(json, strlen(json), "04A2B3C4", s));
    TEST_ASSERT_EQUAL_STRING("", s.name);
    TEST_ASSERT_EQUAL_STRING("", s.vendor);
    TEST_ASSERT_EQUAL_STRING("PLA", s.material);
    TEST_ASSERT_EQUAL_STRING("", s.location);
    TEST_ASSERT_FALSE(s.hasRemaining);
    TEST_ASSERT_TRUE(s.archived);
}

void test_parse_rejects_garbage_and_wrong_shape() {
    SpoolInfo s;
    TEST_ASSERT_EQUAL(ParseResult::Malformed, parseLookup("<html>", 6, "04A2B3C4", s));
    const char* msg = R"({"message": "bad"})";
    TEST_ASSERT_EQUAL(ParseResult::Malformed, parseLookup(msg, strlen(msg), "04A2B3C4", s));
}

void test_parse_truncates_long_strings_safely() {
    std::string longName(200, 'x');
    std::string json = R"([{"id": 1, "filament": {"name": ")" + longName +
                       R"("}, "archived": false, "tags": [{"uid": "04A2B3C4"}]}])";
    SpoolInfo s;
    TEST_ASSERT_EQUAL(ParseResult::Found, parseLookup(json.c_str(), json.size(), "04A2B3C4", s));
    TEST_ASSERT_EQUAL(sizeof(s.name) - 1, strlen(s.name));
}

// ---- formatSummary ----

void test_summary_full() {
    SpoolInfo s;
    parseLookup(FOUND_JSON, strlen(FOUND_JSON), "04A2B3C4", s);
    char text[SUMMARY_BUF];
    formatSummary(s, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("Spoolman #12: eSUN PETG+\n742 g left - K2 CFS", text);
}

void test_summary_minimal_falls_back_to_material_and_marks_archived() {
    const char* json = R"([{"id": 7, "filament": {"material": "PLA"}, "archived": true, "tags": [{"uid": "04A2B3C4"}]}])";
    SpoolInfo s;
    parseLookup(json, strlen(json), "04A2B3C4", s);
    char text[SUMMARY_BUF];
    formatSummary(s, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("Spoolman #7 (archived): PLA\nRemaining unknown", text);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_normalize_strips_separators_and_uppercases);
    RUN_TEST(test_normalize_rejects_non_hex_empty_and_overflow);
    RUN_TEST(test_url_plain);
    RUN_TEST(test_url_trailing_slash_and_whitespace);
    RUN_TEST(test_url_scheme_is_added_when_missing);
    RUN_TEST(test_url_https_is_reported_not_silently_downgraded);
    RUN_TEST(test_url_empty_means_disabled);
    RUN_TEST(test_url_too_long_fails);
    RUN_TEST(test_api_url_any_path);
    RUN_TEST(test_parse_found);
    RUN_TEST(test_parse_empty_list_is_not_found);
    RUN_TEST(test_parse_spool_without_this_tag_is_a_mismatch);
    RUN_TEST(test_parse_picks_the_spool_carrying_the_tag);
    RUN_TEST(test_parse_omitted_optional_fields);
    RUN_TEST(test_parse_rejects_garbage_and_wrong_shape);
    RUN_TEST(test_parse_truncates_long_strings_safely);
    RUN_TEST(test_summary_full);
    RUN_TEST(test_summary_minimal_falls_back_to_material_and_marks_archived);
    return UNITY_END();
}
