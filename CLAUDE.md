# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Standalone RFID programmer for Creality K2 Plus CFS (Creality Filament System). ESP32-S3 firmware using Arduino framework, LVGL 9 UI, LovyanGFX display driver, and PN532 RFID reader/writer for MIFARE Classic 1K tags.

**Target board:** Makerfabs MaTouch ESP32-S3 Parallel TFT 4.3" (SKU: E32S3RGB43) — 800×480 IPS RGB LCD, GT911 capacitive touch, PCF8563 RTC onboard, Mabee I2C + GPIO connectors (Grove HY2.0-4P compatible). No CH422G expander. [Product page](https://www.makerfabs.com/esp32-s3-parallel-tft-with-touch-4-3-inch.html) | [GitHub](https://github.com/Makerfabs/ESP32-S3-Parallel-TFT-with-Touch-4.3inch)

> **Previous board (retired):** Waveshare ESP32-S3 Touch LCD 4.3" — abandoned due to CH422G I2C address collision (occupies 0x20–0x27/0x30–0x3F, including PN532's hardwired 0x24). No usable I2C path to PN532 without board surgery. See `docs/CHECKPOINT.md` for full diagnosis.

## Build Commands

```bash
pio run                          # Build (default env: matouch_s3_43)
pio run -t upload                # Build and flash via UART USB-C port
pio run -t uploadfs              # Upload LittleFS filesystem image
pio run -e matouch_s3_43         # Build for explicit environment
pio device monitor               # Serial monitor (115200 baud)
```

Build requires C++17. Full builds may take 2+ minutes due to linking. The board has two USB-C ports — use the **UART** port for upload/monitor.

## Architecture

### Boot sequence (main.cpp)
`setup()`: Serial → `lvgl_display_init()` (splash) → `filamentDB.init()` (LittleFS JSON) → `network.init()` → `sysState.init()` → `config.init()` → `ui.init()` → `loop()`: `lv_tick_inc()` + `lv_timer_handler()` + `network.process()` + `rfid_task()`.

WiFi, RFID (Auto-Read), and sound (GPIO19) are fully active and integrated into the boot sequence and main loop.

### Module map

| Module | Files | Role |
|--------|-------|------|
| **Display** | `src/lvgl_display.cpp`, `include/LGFX_Config.h` | LovyanGFX + LVGL 9 display/touch driver init |
| **FilamentDB** | `src/filament_db.cpp`, `include/filament_db.h` | Loads `material_database.json` from LittleFS into `std::vector<FilamentProfile>` cache; provides dropdown option strings |
| **SpoolData** | `include/spool_data.h` | CFS tag payload model; constructs from `FilamentProfile` (write path) or raw tag string (read path); 5-char material type on-tag; brand not stored on tag |
| **RFID** | `src/rfid_driver.cpp`, `include/rfid_driver.h` | PN532 driver; Key A derivation from UID; `readCFSTag()`/`writeCFSTag()` |
| **UIManager** | `src/ui/ui_manager.cpp` | Screen management, event handling, `currentSpool`, color picker, `updateDashboardFromSpool()` |
| **Screens** | `src/ui/screens/screen_*.cpp` | Main (3-region layout), Library (filament grid), Settings, About, Filament Select |
| **Config** | `src/config_manager.cpp` | Persistent config (beep, WiFi) via LittleFS `config.json` |
| **Network** | `src/network_manager.cpp` | WiFiManager portal, filament DB updates |
| **State** | `src/system_state.cpp` | SystemState/SystemEvent enums with StateMachine transitions |

### Data flow
1. **Startup:** LittleFS → FilamentDB parses JSON → cache of `FilamentProfile`
2. **Library pick:** Grid tap → `FilamentProfile` → `SpoolData(profile)` → `ui.currentSpool` → `updateDashboardFromSpool()`
3. **Read tag:** Read button → `rfid.readCFSTag(spool)` → `SpoolData(string)` → dashboard update
4. **Write tag:** Write button → `rfid.writeCFSTag(ui.currentSpool)` → status feedback

### Global instances
`filamentDB`, `rfid`, `ui`, `config`, `sysState`, `network` — declared as extern globals, instantiated in their respective .cpp files.

## Data Formats

### Material database (`data/material_database.json` → LittleFS `/material_database.json`)
JSON with `result.list[]` array. Each entry has `base.{id, brand, name, meterialType, colors[]}` and `kvParam.{nozzle_temperature, hot_plate_temp}`. Note: upstream field is `meterialType` (typo). Parsed by `FilamentDB` with ArduinoJson v7 into `std::vector<FilamentProfile>` cache.

### CFS tag payload (SpoolData string)
Fixed-length ASCII string (34+ chars) written to MIFARE Classic 1K via PN532:

| Pos | Len | Field |
|-----|-----|-------|
| 0-4 | 5 | Date code |
| 5-8 | 4 | Vendor ID (`0276` = Creality) |
| 9-10 | 2 | Batch |
| 11 | 1 | Separator (`1`) |
| 12-16 | 5 | Material type, space-padded |
| 17 | 1 | Color prefix (`0`) |
| 18-23 | 6 | Color hex RGB |
| 24-27 | 4 | Length in mm (weight conversion: `len = 330 * weight_g / 1000`) |
| 28-33 | 6 | Serial number |

Entire string uppercased. Brand and filament name are **not stored** on tag. `SpoolData` in `include/spool_data.h` handles both construction paths (from `FilamentProfile` for writes, from raw string for reads).

### RFID sector layout (MIFARE Classic 1K)
Sectors 0-15, 4 blocks/sector, 16 bytes/block. Key A derived from UID. Sectors 1-4 immutable (magic, identity, color, vendor), sector 5 write-once (initial weight), sectors 6-8 mutable mirrors (remaining filament), sector 9 usage counters, sector 15 CRC32. Full spec: `docs/rfid/creality-k2plus-rfid-spec.md`.

### App config (`config.json` on LittleFS)
JSON with `beep_enabled`, WiFi SSID. Managed by `ConfigManager`.

## Key Conventions

- Headers in `include/`, implementations in `src/` (mirrored directory structure including `ui/screens/` and `ui/widgets/`)
- Doxygen `@file`/`@brief` doc blocks in key headers and `main.cpp`
- LVGL 9 API (not v8) — use `lv_obj_*` functions, flex/grid layouts
- LovyanGFX configured in `include/LGFX_Config.h` — pin assignments target MaTouch; no CH422G expander
- LVGL configuration in `include/lv_conf.h`
- Custom board definition in `boards/matouch_s3_43.json`
- ArduinoJson v7 used for JSON parsing (uses PSRAM when available)
- Material type is 5 chars space-padded on RFID tag; trimmed for UI matching
- CFS tag format spec: `docs/rfid/creality-k2plus-rfid-spec.md`

## Hardware Notes (MaTouch ESP32-S3 4.3")

> **Board in use: V3.1** (confirmed from PCB front silkscreen, 2026-03-10). Version number is printed near the Makerfabs logo. V3.1 adds an onboard SPK connector for a small speaker. GPIO pin assignments for I2C, GPIO, and display are the same as V1.3/V2.0. Audio/backlight behaviour for V3.1 TBD — treat as V2.0 (`-DBOARD_MATOUCH_V2`) until confirmed.

- **I2C bus:** GPIO17 (SDA) / GPIO18 (SCL) — shared between GT911 touch, PCF8563 RTC, Mabee I2C port, and PN532. No address conflicts: GT911=0x5D, PCF8563=0x51, PN532=0x24.
- **Mabee I2C port** (HY2.0-4P, Grove-compatible) — pinout confirmed from V3.1 PCB silkscreen:
  - Pin 1 (bottom): GND
  - Pin 2: +3V3
  - Pin 3: SDA → GPIO17
  - Pin 4 (top): SCL → GPIO18
  - PN532 DIP switch: S1=ON, S2=OFF for I2C mode. IRQ/RST not wired through 4-pin connector; library uses polling mode (IRQ=0xFF).
- **Mabee GPIO port** (HY2.0-4P) — GPIO19 / GPIO20. Available on V1.3 and V3.1; used for I2S audio on V2.0.
- **Touch reset:** GPIO38 (V1.3+). Touch INT: not connected (-1).
- **Backlight:** GPIO2 PWM on V1.3. On V2.0: hardware always-on (solder R59 to restore PWM; remove R29 if screen flickers). V3.1: TBD.
- **Audio:**
  - V1.3: no audio hardware. Passive buzzer on Mabee GPIO1 (GPIO19) via LEDC `tone()`.
  - V2.0: I2S on GPIO2 (LRCLK) / GPIO19 (DIN) / GPIO20 (BCLK). Build with `-DBOARD_MATOUCH_V2`.
  - V3.1: onboard SPK connector (+ / -). Likely same I2S as V2.0; use `-DBOARD_MATOUCH_V2` until confirmed.
- **RTC:** PCF8563 onboard — use for timestamps, no NTP dependency.
- **SD card:** SPI on GPIO11 (MOSI) / GPIO12 (SCK) / GPIO13 (MISO), CS = GPIO10. Board ships with 32GB MicroSD.
- **Display:** ESP32-S3 native RGB peripheral → LCD module QT4300H40R10-V03 (panel IC: HX8664/HX8264). Driven in **16-bit RGB565** mode. Pin map: DE=40, VSYNC=41, HSYNC=39, PCLK=42; R0-R4=45,48,47,21,14; G0-G5=5,6,7,15,16,4; B0-B4=8,3,46,9,1. Clock: 16 MHz. Note: "ST7262" label in older docs was incorrect — no such controller chip on this board.
- **PSRAM:** 8MB OPI (`board_build.arduino.memory_type = qio_opi`)
- **Flash:** 16MB
