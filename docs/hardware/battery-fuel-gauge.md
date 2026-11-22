# Battery fuel gauge (MAX17048): spec

Status: **parts ordered 2026-11-22. Driver written on `feat/battery-gauge`; builds, and host tests pass. Not yet run on hardware.**

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

### Driver: direct register reads, no library
`include/battery_gauge.h` / `src/battery_gauge.cpp`, global `batteryGauge`. **Not** `Adafruit_MAX1704X`: its `begin()` sends a soft reset (CMD 0x5400, checked in v1.0.3 source), which would discard the gauge's state of charge on every boot. The driver reads four registers with `Wire`:

| Reg | Meaning | Scale |
|---|---|---|
| 0x02 VCELL | cell voltage | 78.125 µV/LSB |
| 0x04 SOC | state of charge | 1/256 %/LSB |
| 0x08 VERSION | presence check | `0x001x` (0xFFFF = chip present, no battery) |
| 0x16 CRATE | charge rate | signed, 0.208 %/hr/LSB |

- `init()` runs right after the boot I2C scan, before `rfid.init()`. With no gauge, `status().present == false` and the status bar shows nothing. The device works normally without one.
- `tick(now)` runs from `loop()` and polls every **5 s**. It shares the loop task with PN532 auto-read and GT911 touch, so there's no cross-task I2C contention. A gauge missing at boot is re-probed, so hot-plugging works.
- No library dependency, no reset, and chip defaults (hibernate thresholds, alerts) are left alone.

### Classification: `include/battery_logic.h` (pure, `test/test_battery_logic`, 15 cases)
- `Charging`: rate > +0.5 %/hr; stays charging until rate ≤ +0.2.
- `Full`: SoC ≥ 98, |rate| < 0.5, V ≥ 4.15. Overrides charging.
- Level `Low` < 15 %, `Critical` < 5 %, each clearing only 2 % above its threshold.
- Displayed % is clamped to 0–100 and moves at most 1 % per poll. The first reading is shown as-is.
- Icon: `LV_SYMBOL_BATTERY_EMPTY/_1/_2/_3/_FULL` at <10/<35/<60/<85/≥85 %, plus `LV_SYMBOL_CHARGE` while charging. Text: `72%`, `72%+`, or `Full`.
- Colour: green when charging or full, amber when low, red when critical, grey otherwise.
- The thresholds are first guesses. Tune them with the serial `battery` command.

There's no USB-present signal. The gauge only sees the cell, so "plugged in" is inferred from CHARGING/FULL.

### UI wiring
- `ScreenMain::updateBattery(const battery::Status&)` drives `labelBattStatus`. It is called from the 2 s status-bar refresh in `loop()` via `UIManager::updateBatteryStatus()`. With no gauge the label text is empty rather than hidden, so the status text stays centred.
- **Not wired, on purpose:** `SystemEvent::BATTERY_CRITICAL` → `SystemState::LOW_BATTERY`. That state only exits on `WAKE_UP`, which nothing sends, so firing it would strand the UI. A low-battery state needs its own design (what's blocked, how to leave) before it's connected.
- The old `UIManager::updateBattery(float)` + `labelBattery` overlay (hidden, Waveshare-era "<1 V = USB") is untouched and unused.

### Serial console
`battery` prints `present / V / % / rate / charge state / level`.

## Verification on hardware
1. The boot scan lists 0x36 together with 0x24, 0x51, and 0x5D.
2. PN532 auto-read and write still work with the gauge on the bus.
3. `battery` shows a sane voltage (3.0–4.2 V) and an SoC that tracks over about 30 min.
4. On USB: rate goes positive and the charging icon shows. On unplug: within about 1 min it flips to discharging.
5. Unplug the gauge: firmware boots normally with the battery UI hidden.
