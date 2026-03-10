# Checkpoint: Hardware platform migration — Waveshare → Makerfabs MaTouch 4.3"

**Date:** 2026-03-08 (planned) → **2026-03-10 (complete — board received, firmware migrated)**
**Build:** `pio run` — SUCCESS (matouch_s3_43)

---

## Summary

Retired the Waveshare ESP32-S3 Touch LCD 4.3" due to an unresolvable hardware conflict. Migrated to the **Makerfabs MaTouch ESP32-S3 Parallel TFT 4.3"** (SKU: E32S3RGB43). Board received 2026-03-10; firmware migration complete.

---

## Root cause: Waveshare board retired

The CH422G I/O expander (U10, GPIO8/GPIO9) on the Waveshare board responds to **all** I2C addresses in `0x20–0x27` and `0x30–0x3F`. The PN532 has a hardwired I2C address of `0x24`, inside the CH422G range — bus corruption is unavoidable. SPI workaround required soldering to SD card socket PCB pads; not repeatable for multiple units.

---

## New board: Makerfabs MaTouch ESP32-S3 4.3" (E32S3RGB43)

| Property | Value |
|----------|-------|
| MCU | ESP32-S3-WROOM-1-N16R8 (16MB flash, 8MB OPI PSRAM) |
| Display | 4.3" IPS, 800×480, ST7262, 16-bit parallel RGB |
| Touch | GT911 capacitive, I2C on GPIO17/18 |
| RTC | PCF8563 onboard (same I2C bus) |
| Mabee I2C | GPIO17 (SDA) / GPIO18 (SCL), HY2.0-4P Grove-compatible |
| Mabee GPIO | GPIO19 / GPIO20 |
| Backlight | GPIO2 (PWM, V1.3) |
| Touch RST | GPIO38 |
| SD | GPIO10 (CS) / GPIO11 (MOSI) / GPIO12 (SCK) / GPIO13 (MISO) |
| Price | $34.90 |
| Product page | https://www.makerfabs.com/esp32-s3-parallel-tft-with-touch-4-3-inch.html |

---

## PN532 wiring (plug-and-play, no soldering)

1. Set PN532 DIP switch: **S1=ON, S2=OFF** (I2C mode)
2. Grove-to-DuPont adapter cable: HY2.0-4P end → Mabee I2C port
3. DuPont end → PN532 SDA / SCL / 3V3 / GND pins

PN532 I2C address: `0x24`. Onboard devices: GT911 @ `0x14` or `0x5D`, PCF8563 @ `0x51`. No conflict.

---

## Firmware migration completed (2026-03-10)

| Item | Change |
|------|--------|
| `platformio.ini` | Env renamed `matouch_s3_43`; CH422G libs removed |
| `boards/matouch_s3_43.json` | New board definition (Makerfabs vendor, URL) |
| `include/LGFX_Config.h` | All pin assignments updated to MaTouch map; Light_PWM GPIO2; touch RST GPIO38 |
| `src/main.cpp` | `Wire.begin(17, 18)` — MaTouch I2C pins |
| `src/lvgl_display.cpp` | CH422G init block removed |
| `src/rfid_driver.cpp` | Reverted SPI → I2C mode (`Wire`); GPIO17/18 |
| CH422G library | Removed — no CH422G on MaTouch |

---

## Status

| Item | Status |
|------|--------|
| Board received | ✅ 2026-03-10 |
| Firmware (MaTouch target) | ✅ Migration complete |
| LGFX_Config.h migration | ✅ Pins updated |
| rfid_driver.cpp I2C mode | ✅ Reverted from SPI |
| Hardware wiring | ⏳ Verify on bench |

---

# Checkpoint: Post-migration & UI layout

**Date:** 2026-02-07  
**Build:** `pio run` — SUCCESS (waveshare_s3_43) *(historical — env since renamed to matouch_s3_43)*

---

## Summary

Checkpoint after successful migration to the **4.3 development board** (Waveshare ESP32-S3 Touch LCD 4.3") and fixes to the **data model** (FilamentProfile, SpoolData, FilamentDB). **UI layout** evolved to a 3-region main screen; spool widget removed.

---

## Build & hardware

| Item | Value |
|------|--------|
| Environment | `waveshare_s3_43` *(historical — since renamed to matouch_s3_43)* |
| Platform | espressif32 |
| Framework | Arduino |
| Display stack | LovyanGFX + LVGL 9 |
| RFID | PN532 (MIFARE Classic 1K) |
| Board doc | [docs/board-variant-4.3C.md](board-variant-4.3C.md) |

---

## Data model (current)

- **FilamentProfile** (`include/filament_profile.h`)  
  Single catalog entry: id, brand, name, material_type, color_hex/color_name, nozzle_temp, bed_temp, weight_g (default 1000).

- **FilamentDB** (`include/filament_db.h`, `src/filament_db.cpp`)  
  Loads `material_database.json` from LittleFS into a cache of `FilamentProfile`. Exposes:
  - `getAllFilaments()`, `getCache()`
  - `getProfileById(id)` and `getProfileById(id, out)` (bool overload for “found” vs “not found”)
  - `getBrandOptionsForDropdown()`, `getMaterialTypeOptionsForDropdown()`

- **SpoolData** (`include/spool_data.h`)  
  CFS tag payload: construct from `FilamentProfile` (write path) or from raw RFID string (read path). Material type 5 chars on-tag; brand not on-tag. **Trim:** material type and brand are trimmed so they match UI dropdown options; type is also trimmed when loading from JSON and when reading from tag.

---

## Recent changes (UI layout)

1. **Main screen: 3 regions**
   - **Left:** Color block (tap opens color picker).
   - **Right:** Brand dropdown, type dropdown, weight slider.
   - **Bottom:** Grey bar with Read, Write, Library, Settings; write-status label above buttons.

2. **Spool widget removed** — Replaced by color block; widget structure kept for future use.

3. **Read RFID button** — Reads tag into current spool; status shows "Read OK" or "No tag / Read failed".

4. **Write status** — Label above bottom buttons: "Ready", "Write OK", "Write failed", "Read OK", etc.

5. **Color picker** — Larger modal (360×340), 56×56 color swatches, 56×56 close button.

---

## Recent changes (model)

1. **SpoolData**
   - Added `trim_copy()`; material type and brand trimmed when building from profile or from tag string.
   - `setType()` trims input before storing and regenerating the spool string.

2. **FilamentDB**
   - `getProfileById(id, out)` returns `bool` (true if found); original `getProfileById(id)` still returns a default profile when not found.
   - Material type trimmed when loading from JSON (`material_type.trim()`).

3. **Embedded docs**
   - File-level comments added to `filament_db.h`, `filament_profile.h`, `spool_data.h`, `rfid_driver.h`, `ui_manager.h`, `main.cpp`.

4. **Markdown docs**
   - This checkpoint (`docs/CHECKPOINT.md`).
   - Code overview (`docs/CODE-OVERVIEW.md`).
   - README updated for 4.3 dev board, PN532, LovyanGFX, LVGL 9.

---

## How to build / flash

```bash
pio run
pio run -t upload
```

---

## Next steps (suggested)

- CH422G (U10) init for backlight/touch reset per [board-variant-4.3C.md](board-variant-4.3C.md).
- Re-enable WiFi/network and RFID init in `main.cpp` when hardware is ready.

---

# Checkpoint: PN532 SPI migration

**Date:** 2026-02-22
**Build:** `pio run` — SUCCESS (waveshare_s3_43) *(historical — env since renamed to matouch_s3_43)*

---

## Summary

Investigated and resolved the PN532 RFID reader hardware interface conflict. I2C mode is permanently broken on this board due to a CH422G address collision. Migrated `rfid_driver.cpp` to SPI mode using the shared SD card SPI bus and the only free GPIO (GPIO6 via J6).

---

## Root cause: CH422G / PN532 I2C address collision

The CH422G I/O expander (U10, GPIO8 SDA / GPIO9 SCL) responds to **all** 7-bit addresses in the range `0x20–0x27` and `0x30–0x3F`. The PN532 has a hardwired I2C address of `0x24`, which falls inside the CH422G range. Every I2C transaction to `0x24` is answered by both chips simultaneously, causing bus corruption (`i2cRead returned Error -1`).

**This is a hardware-level conflict and cannot be worked around in software on the same I2C bus.** A TCA9548A I2C mux would require PCB-trace surgery and an additional component; not practical.

---

## GPIO constraint analysis

Confirmed from the ESP32-S3-WROOM-1 datasheet (pin table, pages 10–12):

- ESP32-S3 has **no GPIO22 or GPIO23** — the chip's GPIO range is 0–21 and 26–48 only.
- All GPIOs accounted for:

| GPIO(s) | Used by |
|---------|---------|
| 0,1,2,3,5,7,10,14,17,18,21,38,39,40,41,42,45,46,47,48 | 16-bit parallel RGB display |
| 4,8,9 | GT911 touch INT/SDA/SCL |
| 11,12,13 | SD card SPI (MOSI/SCK/MISO) — SD CS via CH422G EXIO4 |
| 15,16 | RS-485 TX/RX; repurposed for green/red LED feedback |
| 19,20 | USB D-/D+ |
| 43,44 | UART0 TX/RX (debug Serial, upload) |
| 26–37 | Internal PSRAM/flash (WROOM-1 ESP32-S3R8 variant) |
| **6** | **Free — J6 Sensor AD "AD" pin (only accessible free GPIO)** |

**GPIO6 is the only free GPIO on this board**, exposed via the J6 Sensor AD connector.

---

## Solution: SPI mode via SD card bus + GPIO6 CS

The SD card SPI signals (GPIO11/12/13) are unmanaged by firmware (SD card not yet used; SD CS is via CH422G EXIO4). These signals are accessible at the SD card socket pads on the PCB.

**SPI pin assignments:**

| Signal | GPIO | Physical access |
|--------|------|----------------|
| PN532 SS/CS | GPIO6 | J6 Sensor AD "AD" pin (directly accessible) |
| SPI SCK | GPIO12 | SD card socket CLK leg (solder to PCB pad) |
| SPI MOSI | GPIO11 | SD card socket DI leg (solder to PCB pad) |
| SPI MISO | GPIO13 | SD card socket DO leg (solder to PCB pad) |
| VCC | 3.3V | J6 Sensor AD "3V3" pin |
| GND | GND | J6 Sensor AD "GND" pin |

**PN532 DIP switch setting for SPI:** SEL0=0 (LOW), SEL1=1 (HIGH)

MicroSD SPI pinout for locating socket pads: pin2=DI(MOSI), pin5=CLK, pin7=DO(MISO).

---

## Firmware changes made

**`src/rfid_driver.cpp`:**
- Removed I2C mode: `TwoWire I2C_PN532`, IRQ/RESET/SDA/SCL pin defines removed
- Added SPI mode: `SPI.begin(SCK=12, MISO=13, MOSI=11, CS=-1)` + `Adafruit_PN532(PN532_SS=6, &SPI)`
- Added `#include <SPI.h>`
- `init()` rewritten; all MIFARE read/write logic unchanged

---

## Status when paused

| Item | Status |
|------|--------|
| Firmware build | ✅ Clean (`pio run`) |
| rfid_driver.cpp | ✅ SPI mode implemented |
| Hardware wiring | ⏳ Not yet done — requires soldering to SD socket pads |
| PN532 DIP switches | ⏳ Need to flip to SPI mode |
| RFID read/write test | ⏳ Blocked on hardware wiring |
| GPIO46 conflict | ⚠️ GPIO46 used by both display HSYNC and feedback buzzer — investigate before enabling buzzer |

---

## Next steps (RFID)

1. Flip PN532 DIP switches: SEL0=LOW, SEL1=HIGH
2. Solder 30 AWG wire from PN532 SCK/MOSI/MISO to SD card socket pads (GPIO12/11/13)
3. Connect PN532 SS → J6 AD (GPIO6), VCC → J6 3V3, GND → J6 GND
4. `pio run -t upload` and check serial for "PN532 found on SPI bus"
5. Investigate GPIO46 buzzer conflict (display uses GPIO46 as HSYNC in LGFX_Config.h)
