// Host tests for the fuel-gauge classification: pio test -e native
#include <unity.h>
#include <cstring>
#include "battery_logic.h"

using namespace battery;

void setUp() {}
void tearDown() {}

static Reading rd(float v, float pct, float rate) { return Reading{v, pct, rate}; }

// ---- charge state ----

void test_positive_rate_is_charging() {
    TEST_ASSERT_EQUAL(ChargeState::Charging, classifyCharge(rd(3.9f, 60, 12.0f), ChargeState::Unknown));
}

void test_negative_rate_is_discharging() {
    TEST_ASSERT_EQUAL(ChargeState::Discharging, classifyCharge(rd(3.8f, 55, -8.0f), ChargeState::Unknown));
}

void test_small_positive_rate_does_not_enter_charging() {
    TEST_ASSERT_EQUAL(ChargeState::Discharging, classifyCharge(rd(3.8f, 55, 0.3f), ChargeState::Discharging));
}

void test_charging_holds_through_small_rate_hysteresis() {
    TEST_ASSERT_EQUAL(ChargeState::Charging, classifyCharge(rd(3.9f, 60, 0.3f), ChargeState::Charging));
}

void test_charging_exits_below_hysteresis() {
    TEST_ASSERT_EQUAL(ChargeState::Discharging, classifyCharge(rd(3.9f, 60, 0.1f), ChargeState::Charging));
}

void test_full_when_high_soc_flat_rate_high_voltage() {
    TEST_ASSERT_EQUAL(ChargeState::Full, classifyCharge(rd(4.18f, 99, 0.2f), ChargeState::Charging));
}

void test_not_full_when_voltage_low() {
    TEST_ASSERT_EQUAL(ChargeState::Discharging, classifyCharge(rd(4.05f, 99, 0.2f), ChargeState::Unknown));
}

void test_full_unplugged_becomes_discharging() {
    TEST_ASSERT_EQUAL(ChargeState::Discharging, classifyCharge(rd(4.16f, 99, -4.0f), ChargeState::Full));
}

// ---- level ----

void test_level_ok_low_critical() {
    TEST_ASSERT_EQUAL(Level::Ok, classifyLevel(50, Level::Ok));
    TEST_ASSERT_EQUAL(Level::Low, classifyLevel(14, Level::Ok));
    TEST_ASSERT_EQUAL(Level::Critical, classifyLevel(4, Level::Low));
}

void test_level_recovers_only_past_hysteresis() {
    TEST_ASSERT_EQUAL(Level::Low, classifyLevel(16, Level::Low));       // 15 + 2 not yet reached
    TEST_ASSERT_EQUAL(Level::Ok, classifyLevel(17, Level::Low));
    TEST_ASSERT_EQUAL(Level::Critical, classifyLevel(6, Level::Critical));
    TEST_ASSERT_EQUAL(Level::Low, classifyLevel(7, Level::Critical));
}

// ---- displayed percent ----

void test_first_reading_sets_display_directly() {
    TEST_ASSERT_EQUAL_FLOAT(73.0f, smoothPercent(0, 73.0f, true));
}

void test_display_steps_at_most_one_percent() {
    TEST_ASSERT_EQUAL_FLOAT(51.0f, smoothPercent(50, 58.0f, false));
    TEST_ASSERT_EQUAL_FLOAT(49.0f, smoothPercent(50, 40.0f, false));
    TEST_ASSERT_EQUAL_FLOAT(50.4f, smoothPercent(50, 50.4f, false));
}

void test_display_clamped_to_0_100() {
    TEST_ASSERT_EQUAL_FLOAT(100.0f, smoothPercent(0, 104.2f, true));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, smoothPercent(0, -1.0f, true));
    TEST_ASSERT_EQUAL_FLOAT(100.0f, smoothPercent(100, 103.0f, false));
}

// ---- icon bars ----

void test_bars_buckets() {
    TEST_ASSERT_EQUAL(0, batteryBars(5));
    TEST_ASSERT_EQUAL(1, batteryBars(10));
    TEST_ASSERT_EQUAL(1, batteryBars(34));
    TEST_ASSERT_EQUAL(2, batteryBars(35));
    TEST_ASSERT_EQUAL(3, batteryBars(60));
    TEST_ASSERT_EQUAL(4, batteryBars(85));
    TEST_ASSERT_EQUAL(4, batteryBars(100));
}

// ---- status text ----

void test_status_text() {
    char buf[32];
    formatBatteryText(Status{true, 3.9f, 72.4f, 10, ChargeState::Charging, Level::Ok}, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("72%+", buf);
    formatBatteryText(Status{true, 3.8f, 54.6f, -5, ChargeState::Discharging, Level::Ok}, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("55%", buf);
    formatBatteryText(Status{true, 4.18f, 100, 0, ChargeState::Full, Level::Ok}, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Full", buf);
    formatBatteryText(Status{}, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("", buf);
}

// ---- icon style: ghosted at 0 % / no gauge, green otherwise, amber/red warnings ----

static Status st(float pct, ChargeState c = ChargeState::Discharging, Level l = Level::Ok, bool present = true) {
    Status s; s.present = present; s.voltage = 3.8f; s.percent = pct; s.charge = c; s.level = l; return s;
}

void test_icon_no_gauge_is_ghosted_empty() {
    IconStyle i = iconStyle(st(50, ChargeState::Unknown, Level::Ok, false));
    TEST_ASSERT_TRUE(i.ghost);
    TEST_ASSERT_EQUAL(0, i.bars);
}

void test_icon_zero_percent_is_ghosted() {
    IconStyle i = iconStyle(st(0.4f, ChargeState::Discharging, Level::Critical));
    TEST_ASSERT_TRUE(i.ghost);
    TEST_ASSERT_EQUAL(0, i.bars);
}

void test_icon_partial_is_green_with_bars() {
    IconStyle i = iconStyle(st(50));
    TEST_ASSERT_FALSE(i.ghost);
    TEST_ASSERT_EQUAL_HEX32(COLOR_GREEN, i.color);
    TEST_ASSERT_EQUAL(2, i.bars);
}

void test_icon_one_percent_is_green_not_ghost() {
    IconStyle i = iconStyle(st(1, ChargeState::Discharging, Level::Ok));
    TEST_ASSERT_FALSE(i.ghost);
}

void test_icon_full_is_green_full_bars() {
    IconStyle i = iconStyle(st(100, ChargeState::Full));
    TEST_ASSERT_EQUAL_HEX32(COLOR_GREEN, i.color);
    TEST_ASSERT_EQUAL(4, i.bars);
}

void test_icon_low_is_amber_critical_is_red() {
    TEST_ASSERT_EQUAL_HEX32(COLOR_AMBER, iconStyle(st(12, ChargeState::Discharging, Level::Low)).color);
    TEST_ASSERT_EQUAL_HEX32(COLOR_RED, iconStyle(st(3, ChargeState::Discharging, Level::Critical)).color);
}

void test_icon_charging_overrides_low_to_green() {
    TEST_ASSERT_EQUAL_HEX32(COLOR_GREEN, iconStyle(st(3, ChargeState::Charging, Level::Critical)).color);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_positive_rate_is_charging);
    RUN_TEST(test_negative_rate_is_discharging);
    RUN_TEST(test_small_positive_rate_does_not_enter_charging);
    RUN_TEST(test_charging_holds_through_small_rate_hysteresis);
    RUN_TEST(test_charging_exits_below_hysteresis);
    RUN_TEST(test_full_when_high_soc_flat_rate_high_voltage);
    RUN_TEST(test_not_full_when_voltage_low);
    RUN_TEST(test_full_unplugged_becomes_discharging);
    RUN_TEST(test_level_ok_low_critical);
    RUN_TEST(test_level_recovers_only_past_hysteresis);
    RUN_TEST(test_first_reading_sets_display_directly);
    RUN_TEST(test_display_steps_at_most_one_percent);
    RUN_TEST(test_display_clamped_to_0_100);
    RUN_TEST(test_bars_buckets);
    RUN_TEST(test_status_text);
    RUN_TEST(test_icon_no_gauge_is_ghosted_empty);
    RUN_TEST(test_icon_zero_percent_is_ghosted);
    RUN_TEST(test_icon_partial_is_green_with_bars);
    RUN_TEST(test_icon_one_percent_is_green_not_ghost);
    RUN_TEST(test_icon_full_is_green_full_bars);
    RUN_TEST(test_icon_low_is_amber_critical_is_red);
    RUN_TEST(test_icon_charging_overrides_low_to_green);
    return UNITY_END();
}
