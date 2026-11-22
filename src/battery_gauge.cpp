#include "battery_gauge.h"
#include <Wire.h>

BatteryGauge batteryGauge;

namespace {
// MAX17048 registers (16-bit, MSB first)
constexpr uint8_t REG_VCELL   = 0x02;   // 78.125 uV / LSB
constexpr uint8_t REG_SOC     = 0x04;   // 1/256 % / LSB
constexpr uint8_t REG_VERSION = 0x08;   // 0x001x on MAX17048/49
constexpr uint8_t REG_CRATE   = 0x16;   // signed, 0.208 %/hr / LSB
}  // namespace

bool BatteryGauge::readReg(uint8_t reg, uint16_t& out) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(I2C_ADDR, (uint8_t)2) != 2) return false;
    const uint8_t hi = Wire.read();
    const uint8_t lo = Wire.read();
    out = (uint16_t(hi) << 8) | lo;
    return true;
}

bool BatteryGauge::probe() {
    uint16_t ver = 0;
    // 0xFFFF means the chip answered but has no battery attached.
    return readReg(REG_VERSION, ver) && (ver & 0xFFF0) == 0x0010;
}

void BatteryGauge::init() {
    status_ = battery::Status{};
    havePercent_ = false;
    if (probe() && poll()) {
        Serial.printf("Battery gauge: MAX17048 found, %.2fV %.1f%% %.1f%%/hr\n",
                      status_.voltage, status_.percent, status_.rateHr);
    } else {
        Serial.println("Battery gauge: not found (0x36), battery UI hidden");
    }
    lastPollMs_ = millis();
}

bool BatteryGauge::poll() {
    uint16_t vcell, soc, crate;
    if (!readReg(REG_VCELL, vcell) || !readReg(REG_SOC, soc) || !readReg(REG_CRATE, crate)) {
        status_.present = false;
        return false;
    }
    const battery::Reading r{ vcell * 78.125e-6f, soc / 256.0f, int16_t(crate) * 0.208f };
    status_.present = true;
    status_.voltage = r.voltage;
    status_.rateHr = r.rateHr;
    status_.percent = battery::smoothPercent(status_.percent, r.percent, !havePercent_);
    havePercent_ = true;
    status_.charge = battery::classifyCharge(r, status_.charge);
    status_.level = battery::classifyLevel(status_.percent, status_.level);
    return true;
}

void BatteryGauge::tick(uint32_t nowMs) {
    if (nowMs - lastPollMs_ < POLL_MS) return;
    lastPollMs_ = nowMs;
    // A gauge that was absent at boot is re-probed, so plugging one in later works without a reboot.
    if (!status_.present && !probe()) return;
    poll();
}
