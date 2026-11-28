#pragma once
/**
 * @file battery_logic.h
 * @brief Turns MAX17048 readings into what the status bar shows: charge state, level, icon, text.
 *
 * There is no USB-present signal: the gauge only sees the cell, so "plugged in" is inferred from
 * the charge rate. Thresholds are first guesses (docs/hardware/battery-fuel-gauge.md); tune them
 * against the serial `battery` command. Pure; host-tested.
 */
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace battery {

enum class ChargeState { Unknown, Discharging, Charging, Full };
enum class Level { Ok, Low, Critical };

struct Reading {
    float voltage;   // V
    float percent;   // raw SoC, may exceed 100
    float rateHr;    // %/hr, + = charging
};

struct Status {
    bool present = false;
    float voltage = 0;
    float percent = 0;   // displayed: clamped and step-limited
    float rateHr = 0;
    ChargeState charge = ChargeState::Unknown;
    Level level = Level::Ok;
};

constexpr float CHARGE_ENTER_RATE = 0.5f;   // %/hr
constexpr float CHARGE_EXIT_RATE  = 0.2f;
constexpr float FULL_PERCENT      = 98.0f;
constexpr float FULL_MAX_RATE     = 0.5f;
constexpr float FULL_MIN_VOLTAGE  = 4.15f;
constexpr float LOW_PERCENT       = 15.0f;
constexpr float CRITICAL_PERCENT  = 5.0f;
constexpr float LEVEL_HYSTERESIS  = 2.0f;
constexpr float MAX_STEP_PERCENT  = 1.0f;   // per update

inline ChargeState classifyCharge(const Reading& r, ChargeState prev) {
    if (r.percent >= FULL_PERCENT && std::fabs(r.rateHr) < FULL_MAX_RATE && r.voltage >= FULL_MIN_VOLTAGE)
        return ChargeState::Full;
    const float enter = prev == ChargeState::Charging ? CHARGE_EXIT_RATE : CHARGE_ENTER_RATE;
    return r.rateHr > enter ? ChargeState::Charging : ChargeState::Discharging;
}

inline Level classifyLevel(float percent, Level prev) {
    if (percent < CRITICAL_PERCENT) return Level::Critical;
    if (prev == Level::Critical && percent < CRITICAL_PERCENT + LEVEL_HYSTERESIS) return Level::Critical;
    if (percent < LOW_PERCENT) return Level::Low;
    if (prev != Level::Ok && percent < LOW_PERCENT + LEVEL_HYSTERESIS) return Level::Low;
    return Level::Ok;
}

inline float smoothPercent(float prevDisplayed, float raw, bool first) {
    float target = raw < 0 ? 0 : (raw > 100 ? 100 : raw);
    if (first) return target;
    const float d = target - prevDisplayed;
    if (d > MAX_STEP_PERCENT) return prevDisplayed + MAX_STEP_PERCENT;
    if (d < -MAX_STEP_PERCENT) return prevDisplayed - MAX_STEP_PERCENT;
    return target;
}

/** 0..4, mapping to LV_SYMBOL_BATTERY_EMPTY / _1 / _2 / _3 / _FULL. */
inline int batteryBars(float percent) {
    if (percent < 10) return 0;
    if (percent < 35) return 1;
    if (percent < 60) return 2;
    if (percent < 85) return 3;
    return 4;
}

/** Short text beside the icon: "72%", "72%+" while charging, "Full", or "" with no gauge. */
inline void formatBatteryText(const Status& s, char* out, size_t n) {
    if (!s.present) { if (n) out[0] = '\0'; return; }
    if (s.charge == ChargeState::Full) { std::snprintf(out, n, "Full"); return; }
    std::snprintf(out, n, "%d%%%s", (int)std::lround(s.percent), s.charge == ChargeState::Charging ? "+" : "");
}

constexpr uint32_t COLOR_GREEN = 0x00C853;
constexpr uint32_t COLOR_AMBER = 0xFFB300;
constexpr uint32_t COLOR_RED   = 0xFF5252;
constexpr uint32_t COLOR_GHOST = 0x808080;

/** How the status-bar battery icon is drawn. `ghost` = faded outline (no gauge, or 0 %). */
struct IconStyle {
    int bars;         // batteryBars()
    uint32_t color;   // RGB
    bool ghost;
};

/**
 * Ghosted empty battery with no gauge or at 0 % (as displayed, rounded); otherwise the fill
 * level in green, amber when Low and red when Critical. Charging or Full stays green.
 */
inline IconStyle iconStyle(const Status& s) {
    if (!s.present || std::lround(s.percent) <= 0) return IconStyle{0, COLOR_GHOST, true};
    uint32_t color = COLOR_GREEN;
    const bool charging = s.charge == ChargeState::Charging || s.charge == ChargeState::Full;
    if (!charging && s.level == Level::Critical) color = COLOR_RED;
    else if (!charging && s.level == Level::Low) color = COLOR_AMBER;
    return IconStyle{batteryBars(s.percent), color, false};
}

}  // namespace battery
