#pragma once
/**
 * @file battery_gauge.h
 * @brief MAX17048 LiPo fuel gauge on the shared I2C bus (0x36). Polled from loop().
 *
 * Reads registers directly instead of using Adafruit_MAX1704X: that library's begin() sends a
 * soft reset, which would throw away the gauge's state of charge on every boot (the gauge stays
 * on the battery while the device is off). No gauge on the bus is normal: status().present is
 * false and the battery UI stays hidden. Spec: docs/hardware/battery-fuel-gauge.md
 */
#include <Arduino.h>
#include "battery_logic.h"

class BatteryGauge {
public:
    static constexpr uint8_t I2C_ADDR = 0x36;
    static constexpr uint32_t POLL_MS = 5000;

    /** Call after Wire.begin(). Probes for the chip; never resets it. */
    void init();
    /** Reads the gauge every POLL_MS; cheap to call every loop. */
    void tick(uint32_t nowMs);
    const battery::Status& status() const { return status_; }

private:
    bool probe();
    bool readReg(uint8_t reg, uint16_t& out);
    bool poll();

    battery::Status status_;
    uint32_t lastPollMs_ = 0;
    bool havePercent_ = false;
};

extern BatteryGauge batteryGauge;
