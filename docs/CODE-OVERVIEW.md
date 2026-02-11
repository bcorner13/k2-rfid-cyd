# Code overview

High-level map of modules and data flow for K2-RFID-CYD. For in-code details, see the `@file` / `@brief` comments in the headers listed below.

---

## Entry and display

| File | Role |
|------|------|
| `src/main.cpp` | `setup()`: Serial, LVGL display init, FilamentDB init, config, UIManager; `loop()`: `lv_timer_handler()`, `ui.update()`. |
| `src/lvgl_display.cpp` | LovyanGFX + LVGL 9 init, display driver, input (touch). |
| `include/LGFX_Config.h` | Panel/bus/touch config for 4.3C (RGB, GT911). |

---

## Data model

| File | Role |
|------|------|
| `include/filament_profile.h` | **FilamentProfile** struct: id, brand, name, material_type, color, temps, weight_g. |
| `include/filament_db.h` / `src/filament_db.cpp` | **FilamentDB**: load `material_database.json` from LittleFS, cache of profiles, dropdown option strings, `getProfileById`. |
| `include/spool_data.h` | **SpoolData**: CFS tag payload; construct from FilamentProfile or from raw tag string; 5-char material type on-tag; trim for UI matching. |

---

## RFID

| File | Role |
|------|------|
| `include/rfid_driver.h` / `src/rfid_driver.cpp` | **RFIDDriver**: PN532, Key A derivation, `readCFSTag(SpoolData&)`, `writeCFSTag(const SpoolData&)`. |
| `docs/rfid/creality-k2plus-rfid-spec.md` | CFS tag layout and sector usage. |

---

## UI (LVGL 9)

| File | Role |
|------|------|
| `include/ui/ui_manager.h` / `src/ui/ui_manager.cpp` | **UIManager**: screens, event_handler, currentSpool, color picker, updateDashboardFromSpool. |
| `include/ui/screens/screen_main.h` | Main screen: **3 regions** — left: color block (tap opens picker); right: brand/type dropdowns, weight slider; bottom: Read, Write, Library, Settings, write-status label. |
| `include/ui/screens/screen_library.h` | Filament library grid (from FilamentDB cache). |
| `include/ui/screens/screen_settings.h` | Settings: WiFi, DB update, beep, About, Restart. |
| `include/ui/screens/screen_about.h` | About screen. |
| `include/ui/widgets/spool_widget.h` | SpoolWidget (legacy/unused; structure kept for future widgets). |

---

## Other

| File | Role |
|------|------|
| `include/config_manager.h` | Persistent config (e.g. beep). |
| `include/network_manager.h` | WiFiManager, filament DB update. |
| `include/system_state.h` | System state (e.g. for future use). |

---

## Data flow (simplified)

1. **Startup:** LittleFS → FilamentDB loads JSON → cache of FilamentProfile.  
2. **Library pick:** User taps item in grid → index into `filamentDB.getCache()` → FilamentProfile → SpoolData(profile) → `ui.currentSpool` → `updateDashboardFromSpool()` → main screen updated.  
3. **Read tag:** User taps Read → `rfid.readCFSTag(spool)` → SpoolData(string) → `updateDashboardFromSpool()` → status "Read OK" or "No tag / Read failed".  
4. **Write tag:** User taps Write → `rfid.writeCFSTag(ui.currentSpool)` → status "Write OK" or "Write failed".  
5. **Main screen:** Left: color block (tap opens color picker); right: brand/type dropdowns, weight slider; bottom: Read, Write, Library, Settings, status label.

---

## Embedded documentation (in-code)

Doxygen-style `@file` / `@brief` blocks are in:

- `include/filament_db.h`
- `include/filament_profile.h`
- `include/spool_data.h`
- `include/rfid_driver.h`
- `include/ui/ui_manager.h`
- `src/main.cpp`

Use these for quick reference when editing; this markdown file gives the bigger picture.
