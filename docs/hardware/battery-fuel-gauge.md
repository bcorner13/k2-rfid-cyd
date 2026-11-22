# Battery fuel gauge (MAX17048): spec

Status: **parts ordered 2026-11-22, not yet wired.** This is the driver spec to build against when they arrive.

## Why

The MaTouch V3.1 charges the 1S 3.7 V LiPo on-board but exposes no state of charge. The old FSD plan (GPIO6 "Sensor AD" ÷3 divider, CS8501) was for the retired Waveshare board. On the MaTouch, GPIO6 is RGB display line G1. See the superseded notes in the FSD and commit `9bdddb9`.

## Parts

| Part | Role |
|---|---|
| Newfame MAX17048 breakout (clone of Adafruit #5580 layout) | Fuel gauge: 2× JST-PH battery pass-through, 2× Qwiic/STEMMA QT, pins VIN/GND/SCL/SDA/INT/QSTRT |
| SparkFun Qwiic ↔ Grove adapter, 100 mm | Board Mabee I2C port → gauge |
| Hiteuoms Qwiic kit (includes 2× Qwiic → female Dupont, 15 cm) | Gauge → PN532 4-pin header |

On arrival, check the chip marking reads **17048**, not 17043. The 17043 has no CRATE register, so it can't report a charge rate.

## Wiring

```
Battery ──JST-PH──▶ gauge BAT A
                    gauge BAT B ──[power switch]──JST──▶ MaTouch battery connector
MaTouch Mabee I2C (Grove) ──SparkFun cable──▶ gauge Qwiic 1
gauge Qwiic 2 ──Qwiic→female Dupont──▶ PN532 header (GND / VCC / SDA / SCL)
```

- The gauge stays on the battery side of the switch, so it keeps its state of charge (SoC) model while the device is off. The switch still stops charging when it is off; that is accepted.
- Check that red lines up with **+** at every JST. Cheap LiPo leads don't follow one polarity convention.
- When shortening battery leads, cut **one wire at a time**, stagger the splices, and heat-shrink each one.
- Don't trust the kit's wire colours: match each wire to the silkscreen at both ends.
- The current PN532 cable (Grove to Grove, with the PN532 end pushed onto its header) becomes a spare.

## I2C bus after the change

| Device | Addr |
|---|---|
| PCF8563 RTC | 0x51 |
| GT911 touch | 0x5D |
| PN532 | 0x24 |
| **MAX17048** | **0x36** |

The bus is `Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL)` = GPIO17/18 in `src/main.cpp:60`. The boot I2C scan in `src/main.cpp` (~line 67) should list 0x36. Use that as the first hardware check.

The gauge board adds its own pull-ups to the bus. If I2C becomes flaky (PN532 auth or reads failing), suspect the combined pull-up resistance before the code. On Adafruit-style boards those pull-ups can be cut off with a solder jumper.

## Firmware

### Library
`adafruit/Adafruit MAX1704X` (add to `lib_deps` in the `matouch_s3_43` env). API used: `begin(&Wire)`, `cellVoltage()`, `cellPercent()`, `chargeRate()` (%/hr, signed), `isDeviceReady()`.

**Verify before relying on it:** whether `begin()` issues a reset or quick-start. Doing that on every boot would discard the gauge's learned state, and the gauge stays powered while the device is off. Read the library source after installing it, and don't reset on boot.

### Module
New `include/battery_gauge.h` + `src/battery_gauge.cpp`, global `battery` (project convention).

- `init()`: after `Wire.begin`, before `ui.init()`. If 0x36 doesn't answer, set `present = false` and keep the battery UI hidden. **The device must keep working with no gauge** (USB-only units and the bench setup).
- `tick(now)`: poll every **5 s** from `loop()`. Don't poll every loop: it shares the bus with PN532 auto-read.
- Exposes a plain struct:

```cpp
struct BatteryStatus {
    bool    present;      // gauge answered on 0x36
    float   voltage;      // V
    float   percent;      // 0–100, clamped (raw SoC can read >100 %)
    float   rateHr;       // %/hr, + = charging
    enum State { UNKNOWN, DISCHARGING, CHARGING, FULL, LOW, CRITICAL } state;
};
```

### Classification (pure, host-testable)
Put this in `include/battery_logic.h`, following the `spoolman_parse.h` pattern, and test it under `pio test -e native` (TDD):

- `CHARGING`: `rateHr > +0.5` (hysteresis: leave the state only when `< +0.2`)
- `FULL`: `percent >= 98` and `|rateHr| < 0.5` and `voltage >= 4.15`
- `LOW`: `percent < 15`; `CRITICAL`: `percent < 5`. Fires `SystemEvent::BATTERY_CRITICAL`.
- `DISCHARGING`: otherwise
- Smooth the displayed % (e.g. EMA or ±1 % step limit) so the icon doesn't flicker.
- Icon bucket: `LV_SYMBOL_BATTERY_EMPTY / _1 / _2 / _3 / _FULL` at <10 / <35 / <60 / <85 / ≥85 %, plus `LV_SYMBOL_CHARGE` when CHARGING.
- The initial thresholds above are guesses. Tune them against real readings.

There's no USB-present signal. The gauge only sees the cell, so "plugged in" is inferred from CHARGING/FULL.

### Existing hooks to wire (already in tree)
- `ScreenMain::updateStatusBar()` (`src/ui/screens/screen_main.cpp:~268`). `labelBattStatus` shows a fixed grey `LV_SYMBOL_BATTERY_FULL` placeholder. Replace it with the real icon, %, and colour.
- `UIManager::updateBattery(float voltage)` + `labelBattery` overlay (`src/ui/ui_manager.cpp:~868–884`). Hidden. Its current "<1 V = USB" logic is a Waveshare-era placeholder. Change the signature to take `BatteryStatus`, or delete it if the status-bar icon is enough.
- The 2 s status-bar refresh in `loop()` (`src/main.cpp:~307`) is where to push the battery state.
- `SystemState::LOW_BATTERY` / `SystemEvent::BATTERY_CRITICAL` (`include/system_state.h`, `src/system_state.cpp:91`) exist but are never fired.

### Serial console
Add a `battery` command alongside `url` / `printer` / `sync` / `state`. It prints present, V, %, rate, and state. Use it to tune the thresholds.

## Verification on hardware
1. The boot scan lists 0x36 together with 0x24, 0x51, and 0x5D.
2. PN532 auto-read and write still work with the gauge on the bus.
3. `battery` shows a sane voltage (3.0–4.2 V) and an SoC that tracks over about 30 min.
4. On USB: rate goes positive and the charging icon shows. On unplug: within about 1 min it flips to discharging.
5. Unplug the gauge: firmware boots normally with the battery UI hidden.
