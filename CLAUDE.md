# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Standalone RFID programmer for Creality K2 Plus CFS (Creality Filament System). ESP32-S3 firmware using Arduino framework, LVGL 9 UI, LovyanGFX display driver, and PN532 RFID reader/writer for MIFARE Classic 1K tags.

**Target board:** Waveshare ESP32-S3 Touch LCD 4.3" (4.3C variant) — 800x480 RGB LCD, GT911 capacitive touch, CH422G I2C expander for backlight/touch reset.

## Build Commands

```bash
pio run                          # Build (default env: waveshare_s3_43)
pio run -t upload                # Build and flash via UART USB-C port
pio run -t uploadfs              # Upload LittleFS filesystem image
pio run -e waveshare_s3_43       # Build for explicit environment
pio device monitor               # Serial monitor (115200 baud)
```

Build requires C++17 (for ESP32_IO_Expander/CH422G). Full builds may take 2+ minutes due to linking. The board has two USB-C ports — use the **UART** port for upload/monitor.

## Architecture

### Boot sequence (main.cpp)
`setup()`: Serial → `lvgl_display_init()` (splash) → `filamentDB.init()` (LittleFS JSON) → `sysState.init()` → `config.init()` → `ui.init()` → screen inits → `loop()`: `lv_tick_inc()` + `lv_timer_handler()` every 5ms.

WiFi, RFID, and sound init are currently commented out pending hardware verification.

### Module map

| Module | Files | Role |
|--------|-------|------|
| **Display** | `src/lvgl_display.cpp`, `include/LGFX_Config.h` | LovyanGFX + LVGL 9 display/touch driver init |
| **FilamentDB** | `src/filament_db.cpp`, `include/filament_db.h` | Loads `material_database.json` from LittleFS into `std::vector<FilamentProfile>` cache; provides dropdown option strings |
| **SpoolData** | `include/spool_data.h` | CFS tag payload model; constructs from `FilamentProfile` (write path) or raw tag string (read path); 5-char material type on-tag; brand not stored on tag |
| **RFID** | `src/rfid_driver.cpp`, `include/rfid_driver.h` | PN532 driver; Key A derivation from UID; `readCFSTag()`/`writeCFSTag()` |
| **UIManager** | `src/ui/ui_manager.cpp` | Screen management, event handling, `currentSpool`, color picker, `updateDashboardFromSpool()` |
| **Screens** | `src/ui/screens/screen_*.cpp` | Main (3-region layout), Library (filament grid), Settings, About, Filament Select |
| **Config** | `src/config_manager.cpp` | Persistent config (beep, brightness, WiFi) via LittleFS `config.json` |
| **Network** | `src/network_manager.cpp` | WiFiManager portal, filament DB updates |
| **State** | `src/system_state.cpp` | SystemState/SystemEvent enums with StateMachine transitions |

### Data flow
1. **Startup:** LittleFS → FilamentDB parses JSON → cache of `FilamentProfile`
2. **Library pick:** Grid tap → `FilamentProfile` → `SpoolData(profile)` → `ui.currentSpool` → `updateDashboardFromSpool()`
3. **Read tag:** Read button → `rfid.readCFSTag(spool)` → `SpoolData(string)` → dashboard update
4. **Write tag:** Write button → `rfid.writeCFSTag(ui.currentSpool)` → status feedback

### Global instances
`filamentDB`, `rfid`, `ui`, `config`, `sysState`, `network` — declared as extern globals, instantiated in their respective .cpp files.

## Key Conventions

- Headers in `include/`, implementations in `src/` (mirrored directory structure including `ui/screens/` and `ui/widgets/`)
- Doxygen `@file`/`@brief` doc blocks in key headers and `main.cpp`
- LVGL 9 API (not v8) — use `lv_obj_*` functions, flex/grid layouts
- LovyanGFX configured in `include/LGFX_Config.h` for the 4.3C variant specifically
- LVGL configuration in `include/lv_conf.h`
- Custom board definition in `boards/waveshare_s3_43.json`
- ArduinoJson v7 used for JSON parsing (uses PSRAM when available)
- Material type is 5 chars space-padded on RFID tag; trimmed for UI matching
- CFS tag format spec: `docs/rfid/creality-k2plus-rfid-spec.md`

## Hardware Notes

- I2C bus (GPIO8 SDA, GPIO9 SCL) shared between GT911 touch, CH422G expander, and PN532 RFID
- CH422G (U10) controls backlight (EXIO_PWM) and touch reset (EXIO1) — init pending
- Display uses 16-bit parallel RGB bus at 12 MHz (lowered to reduce jitter)
- PSRAM: 8MB OPI (`board_build.arduino.memory_type = qio_opi`)
- Flash: 16MB
