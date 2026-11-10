// Host tests for colour names: pio test -e native -f test_color_names
#include <unity.h>
#include <cstring>
#include <ArduinoJson.h>
#include <color_names.h>

void setUp() {}
void tearDown() {}

void test_palette_colors_have_exact_names() {
    bool exact = false;
    TEST_ASSERT_EQUAL_STRING("Black", nearestColorName(0x000000, &exact));
    TEST_ASSERT_TRUE(exact);
    TEST_ASSERT_EQUAL_STRING("Gold", nearestColorName(0xFFD700, &exact));
    TEST_ASSERT_TRUE(exact);
    TEST_ASSERT_EQUAL_STRING("Light Blue", nearestColorName(0xADD8E6, &exact));
    TEST_ASSERT_TRUE(exact);
}

void test_other_colors_get_nearest_name() {
    bool exact = true;
    TEST_ASSERT_EQUAL_STRING("Red", nearestColorName(0xE00000, &exact));
    TEST_ASSERT_FALSE(exact);
    TEST_ASSERT_EQUAL_STRING("Navy", nearestColorName(0x1A237E, &exact));
}

// Shape of material_database.json base.mixColorInfoList (02001 Hyper PLA-CF).
static const char* MIX = R"([
  {"colorEn":"Black","hexValue":"#000000"},
  {"colorEn":"Dark Green","hexValue":"#212721"},
  {"colorEn":"","hexValue":"#74654F"},
  {"colorEn":"Wine Red","hexValue":"#711712"}
])";

void test_product_name_found_by_hex() {
    JsonDocument d;
    deserializeJson(d, MIX);
    TEST_ASSERT_EQUAL_STRING("Dark Green", productColorName(d.as<JsonArrayConst>(), 0x212721).c_str());
    TEST_ASSERT_EQUAL_STRING("Wine Red", productColorName(d.as<JsonArrayConst>(), 0x711712).c_str());
}

void test_product_name_hex_case_insensitive() {
    JsonDocument d;
    deserializeJson(d, R"([{"colorEn":"Sky","hexValue":"#a1b2c3"}])");
    TEST_ASSERT_EQUAL_STRING("Sky", productColorName(d.as<JsonArrayConst>(), 0xA1B2C3).c_str());
}

void test_product_name_missing_or_blank_is_empty() {
    JsonDocument d;
    deserializeJson(d, MIX);
    TEST_ASSERT_EQUAL_STRING("", productColorName(d.as<JsonArrayConst>(), 0x74654F).c_str());   // blank name
    TEST_ASSERT_EQUAL_STRING("", productColorName(d.as<JsonArrayConst>(), 0x123456).c_str());   // not listed
    TEST_ASSERT_EQUAL_STRING("", productColorName(JsonArrayConst(), 0x000000).c_str());         // no list
}

void test_label_uses_product_name() {
    char buf[48];
    formatColorLabel("Dark Green", 0x212721, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Dark Green\n#212721", buf);
}

void test_label_falls_back_to_nearest() {
    char buf[48];
    formatColorLabel("", 0xE00000, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("~Red\n#E00000", buf);
    formatColorLabel(nullptr, 0xFFD700, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Gold\n#FFD700", buf);   // exact palette colour: no "~"
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_palette_colors_have_exact_names);
    RUN_TEST(test_other_colors_get_nearest_name);
    RUN_TEST(test_product_name_found_by_hex);
    RUN_TEST(test_product_name_hex_case_insensitive);
    RUN_TEST(test_product_name_missing_or_blank_is_empty);
    RUN_TEST(test_label_uses_product_name);
    RUN_TEST(test_label_falls_back_to_nearest);
    return UNITY_END();
}
