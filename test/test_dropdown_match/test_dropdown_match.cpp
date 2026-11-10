#include <unity.h>
#include <dropdown_match.h>

void setUp() {}
void tearDown() {}

// The bug: a longer option that starts with the target came first and won.
void test_exact_match_beats_earlier_prefix() {
    TEST_ASSERT_EQUAL_UINT16(1, findDropdownOption("Hyper PLA Matte\nHyper PLA\nCR-Silk", "Hyper PLA"));
    TEST_ASSERT_EQUAL_UINT16(1, findDropdownOption("PLA-CF\nPLA\nPETG", "PLA"));
}

void test_exact_match_found() {
    TEST_ASSERT_EQUAL_UINT16(2, findDropdownOption("PLA\nPETG\nPLA-CF", "PLA-CF"));
}

void test_padded_options_are_trimmed() {
    TEST_ASSERT_EQUAL_UINT16(1, findDropdownOption("  PLA \n PETG  ", "PETG"));
}

void test_prefix_used_only_when_no_exact() {
    TEST_ASSERT_EQUAL_UINT16(1, findDropdownOption("ABS\nHyper PLA Matte", "Hyper PLA"));
}

void test_no_match_returns_zero() {
    TEST_ASSERT_EQUAL_UINT16(0, findDropdownOption("PLA\nPETG", "TPU"));
    TEST_ASSERT_EQUAL_UINT16(0, findDropdownOption("PLA\nPETG", ""));
    TEST_ASSERT_EQUAL_UINT16(0, findDropdownOption(nullptr, "PLA"));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_exact_match_beats_earlier_prefix);
    RUN_TEST(test_exact_match_found);
    RUN_TEST(test_padded_options_are_trimmed);
    RUN_TEST(test_prefix_used_only_when_no_exact);
    RUN_TEST(test_no_match_returns_zero);
    return UNITY_END();
}
