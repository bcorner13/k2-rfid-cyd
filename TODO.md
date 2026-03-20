# K2 RFID Tag Programmer — TODO

---

## 🎯 Current Focus

**Milestone 1: Hardware-Stable Core** — Fix the 9 hardware-observed bugs (collected 2026-03-10) before
advancing features. Goal: RFID read/write works reliably on a real Creality spool, no UI lockouts,
clean boot output.

---

## 🐛 Active Bugs (fix in this order)

Issues collected 2026-03-10 by walking through the physical device. Ordered by impact and dependency.

### ~~Step 1 — Fix Issue #9: PN532 invalid pin log errors~~ ✅ DONE

**Fix applied in `b8edfba`:** `PN532_IRQ` and `PN532_RESET` changed to `(0xFF)` in `src/rfid_driver.cpp`.

---

### ~~Step 2 — Fix Issues #3 & #7: State machine UI lockout~~ ✅ DONE

**What was already in place:** ERROR→IDLE transitions (USER_DISMISS, USER_RETRY, TIMEOUT) in state
machine; `tick()` auto-dismiss; `labelWriteStatus` tap-to-dismiss on Main; `loop()` calls `tick()` and
re-enables buttons.

**Remaining gaps fixed (2026-03-20):**
- `showMainScreen()` now preserves the status label in ERROR state instead of resetting to "Ready".
- DB update failure (`Issue #7`): after `DB_UPDATE_FAILED`, error message is shown on `labelWriteStatus`
  and UI navigates to Main so the user can see it (shown in red) and tap to dismiss. Previously the
  Settings screen buttons went grey for 15 s with no visual feedback.

---

### ~~Step 3 — Fix Issue #2: RFID Key A authentication failure on real Creality spool~~ ✅ DONE

**Code fix already in place** (see `generateKeyA()` comment block, rfid_driver.cpp:940–964):
- UID is cycled into a 16-byte `input[16]` buffer (`uid[i % currentUidLen]`) — no stack overread.
- Separate `aes_out[16]` used for AES output; only 6 bytes copied to `keyOut` — no overflow.
- Algorithm matches DnG-Crafts/K2-RFID Utils.cs CreateKey() reference (cycling, not zero-pad).
- Auth log now prints UID + derived key on one line for cross-checking with reference tools.

**Confirmed on hardware:** Spool successfully read in a prior session. Key A derivation correct.

---

### ~~Step 4 — Fix Issue #1: Splash screen WiFi status not displayed~~ ✅ DONE

**Label update path:** Already working — `lv_tick_set_cb(millis)` ensures LVGL tick advances during
`setup()` so `lv_timer_handler()` actually flushes dirty areas. Each `splash_update_status()` call
already invokes `lv_timer_handler()`.

**Continue button (2026-03-20):** Implemented in `lvgl_display.cpp`:
- `splash_show_continue_button()` creates a tappable button on the splash screen.
- `splash_is_continue_pressed()` returns true when tapped.
- `setup()` calls `splash_show_continue_button()` after all 4 status items are populated.
- The 10-second hold loop breaks immediately on tap; auto-advances at 10 s if not tapped.

---

### ~~Step 5 — Fix Issue #8: First-boot .tmp file errors~~ ✅ DONE (already correct)

`recoverTmpFile()` opens with `if (!LittleFS.exists(tmpPath.c_str())) return;` — silently returns
when neither the base file nor its `.tmp` exists (first boot). `ConfigManager::load()` silently calls
`save()` when `/config.json` is absent. `InventoryManager::init()` logs an informational
"No inventory.json found, starting empty." (not an error) before creating defaults. No log noise.

---

### ~~Step 6 — Fix Issue #6: Audio feedback silent on RFID read/write~~ ✅ DONE

Three root causes fixed (2026-03-20):
1. **`-DBOARD_MATOUCH_V2` added to `platformio.ini`** — activates I2S UISound path (GPIO2/19/20),
   sets `FEEDBACK_HARDWARE_ENABLED=0`, disables backlight PWM (V3.1 hardware always-on like V2.0).
   Also added `-DBOARD_MATOUCH_V31` marker for future conditionals.
2. **`uiSound.init()` + `uiSound.playStartup()` uncommented** in `setup()` — I2S now initialises and
   plays the three-note startup melody on boot.
3. **`feedback.cpp` wired to `uiSound`** for all event methods when `FEEDBACK_HARDWARE_ENABLED=0`:
   - `readSuccess` / `tagDetected` / `spoolSaved` → short tones / click
   - `writeSuccess` / `dbUpdateSuccess` → ascending A5→E6 pair
   - `operationFailed` / `dbUpdateFailed` → low E4 tone (400 ms)
   All respect `config.data.beep_enabled`. V1.3 GPIO buzzer path is unchanged.

---

### ~~Step 7 — Fix Issues #4 & #5: About screen live status + investigate low heap~~ ✅ DONE

**Issue #4 fix (2026-03-20):** Added `labelWifiStatus` and `labelRFIDLive` to `ScreenAbout::show()`:
- WiFi: shows SSID + IP in green, or "Not connected" in red — via `network.isConnected()` / `WiFi.SSID()` / `WiFi.localIP()`.
- RFID: shows "PN532 OK | IC=0x32 ver=1.6" in green, or "not found" in red — via `rfid.getFirmwareVersion()`.
- Memory line now shows PSRAM free KB alongside total.

**Issue #5 resolved (2026-03-20):** Per-step heap/PSRAM logging added to `setup()`. Observed on hardware:
- PSRAM free ≈ 7424KB (out of ~7MB reported usable) — all large allocations correctly in PSRAM.
- Internal heap free ≈ 68KB — normal for ESP32-S3 at this boot stage; FreeRTOS + Arduino overhead
  accounts for the difference. No fragmentation concern.
- **Verdict: memory is healthy.** 68KB internal heap is not low — it's expected when PSRAM carries
  the working set.

---

## ✅ Completed

- [x] **Hardware Migration** — Waveshare retired (CH422G/PN532 I2C conflict); migrated to Makerfabs
  MaTouch ESP32-S3 4.3" V3.1 (confirmed from PCB silkscreen 2026-03-10).
- [x] **Board pins & docs** — `board_pins.h`, `CLAUDE.md`, `screen_about.cpp` updated for V3.1.
  Mabee I2C pinout confirmed: Pin1=GND, Pin2=3V3, Pin3=SDA(GPIO17), Pin4=SCL(GPIO18).
- [x] **RFID I2C mode** — `rfid_driver.cpp` reverted SPI→I2C; polling mode (IRQ=-1, RESET=-1);
  PN532 confirmed found at 0x24 (IC=0x32 ver=1.6). I2C scan shows 0x14 (GT911) + 0x24 (PN532).
- [x] **Audio implementation** — `ui_sound.cpp` dual-path: V1.3 LEDC buzzer via `tone()`;
  V2.0/V3.1 I2S sine wave generator on GPIO2/19/20.
- [x] **RFID Auto-Read** — Background task polls for tags; auto-detect working (tag detected, auth
  attempted). Issue is auth failure, not detection.
- [x] **WiFi Stability** — Non-blocking WiFiManager portal; WDT timeout extended; no reboots during
  portal use.
- [x] **Display Fix** — LVGL buffer moved to internal RAM; PCLK lowered to 12 MHz; screen stable.
- [x] **Persistent Config** — WiFi printer IP saved via `config.json`; `ConfigManager` atomic write.
- [x] **Splash Screen** — 10-second boot window with WiFi status labels (label update path broken —
  see Issue #1).
- [x] **Inventory Manager** — `InventoryManager` with atomic save, UID collision handling, archive/
  delete, weight reconciliation (P0.1, P1.2 from FSD).
- [x] **RFID data integrity** — CRC-32 (P0.2), mirror voting (P0.3), tag v1/v2 detection (P0.7).
- [x] **FilamentDB** — 40 profiles loaded from LittleFS `material_database.json`. ArduinoJson v7.
- [x] **P0 + P1 FSD tiers implemented** (code exists; real-hardware validation in progress per
  Milestone 1 bug fixes above).
- [x] **Issue #9 — PN532 invalid pin log errors** — `PN532_IRQ`/`PN532_RESET` set to `0xFF` sentinel
  (`b8edfba`). No more `Invalid pin selected` boot noise.

---

## 🗺 Milestones

### Milestone 1 — Hardware-Stable Core  *(current)*
Fix all 9 active bugs (Step 1–7 above). Exit criteria:
- RFID reads a real Creality spool without auth failure
- No UI lockout after any error condition; Dismiss/Retry always available
- Clean boot log (no invalid-pin errors, no .tmp errors)
- Audio plays on RFID read success and failure
- Splash shows WiFi status + Continue button

### Milestone 2 — Verified RFID R/W
- RFID **write** tested on a blank MIFARE Classic tag → verified in Creality K2 Plus
- Key A derivation confirmed correct against spec
- CRC32 and mirror sectors written and validated on read-back
- SpoolData fields (date code, vendor, material, color, length, serial) round-trip correctly
- State transitions: IDLE → READING/WRITING → IDLE (no lockout, no stale state)
- **Remaining grams on main screen:** After auto-read, surface `rfid.getLastMirrors()` data
  (`remain_weight_g`) on the dashboard so users can see current vs original weight at a glance.
  Mirror sectors use the standard MIFARE key (confirmed 2026-03-20 on Creality spool).

### Milestone 3 — Inventory Flow
- Scan tag → SpoolRecord auto-added to inventory (or reconciled if UID already exists)
- Library pick → `FilamentProfile` → SpoolRecord → write tag → inventory updated
- Spool Detail screen: weight shown, weight update modal works, history entry logged
- Custom Spool Entry wizard: all 5 steps functional, Save Locally + Save+Write Tag both work
- Inventory screen: list shows all spools with color swatch, weight bar, status dot

### Milestone 4 — Polish & Completeness
- Audio feedback: distinct tones for read-ok / write-ok / error / low-battery; boot melody
- About screen: live WiFi SSID/IP + RFID status (Issue #4)
- Settings screen: beep on/off toggle functional
- Sleep mode: screen timeout + wake on touch (GT911 interrupt)
- Memory audit: PSRAM usage confirmed healthy (Issue #5), heap budget documented
- **Multi-brand support:** Our current `material_database.json` only contains Creality and
  Generic (2 brands). The K2-RFID project's database includes third-party brands (eSUN
  confirmed). Two work items:
  1. **Replace/expand the DB file:** Source the fuller K2-RFID material database which
     includes eSUN and other brands. Re-upload to LittleFS (`pio run -t uploadfs`).
  2. **Library UI:** Brand filter/dropdown must enumerate brands dynamically from
     `FilamentProfile.brand` — not hardcoded — so new brands appear automatically.
  3. **RFID read brand mapping:** Only vendor ID `0276` → Creality is currently mapped.
     Add known third-party vendor IDs as discovered. Unknown IDs show raw ID + allow
     user assignment via Custom Spool Entry.
  Brands confirmed needed: Creality, eSUN, GREETECH, Generic (fallback).
- **Filament specs in UI (temps):** Surface `nozzle_temp` / `bed_temp` from `FilamentProfile`
  on the dashboard and spool detail screen for library-picked spools.
  - Creality v1 RFID tags do NOT store temps on-tag (CFS payload has no temp fields).
  - Our v2 tag format stores temps in sector 10 (`ExtTempBlock`: nozzle min/max/default,
    bed min/max/default) — populated when writing a tag from a library profile.
  - On scan: if tag is v2 + has our origin magic, read temps from sector 10; otherwise
    fall back to DB lookup by material type (best-effort match).
  - Custom Spool Entry wizard needs nozzle temp + bed temp input fields.

### Milestone 5 — Production Prep *(stretch / future)*
- Battery monitoring via GPIO6 ADC (voltage-to-SoC table; low-battery state)
- SD card backup: inventory.json + usage log CSV appended on each write
- V3.1 I2S audio path confirmed (currently using V2.0 path as proxy)
- OTA database update from printer HTTP API tested end-to-end
- P2 FSD items: I2C bus recovery (P2.1), string length enforcement (P2.6), memory monitoring (P2.5)

### Milestone 6 — Bambu Lab NFC Read *(stretch)*
- Bambu Lab AMS spools use **NFC Forum Type 2 tags** (NTAG213/215), not MIFARE Classic.
  The PN532 can read these in passive mode — no Key A auth required.
- Tag format is community-reverse-engineered (not officially documented by Bambu Lab).
  Reference: community work at github.com/Bambu-Research-Group/RFID-Tag-Guide
- **Read-only goal:** detect tag type on scan, parse Bambu payload (material type, color,
  min/max temps, spool weight) and display on dashboard — same UX as a Creality spool read.
- No write support planned (Bambu tags are write-locked after factory programming).
- Implementation notes:
  - `checkTagPresent()` already calls `readPassiveTargetID()`; NTAG returns ATQA 0x0044
    (vs MIFARE Classic 0x0002/0x0004) — use this to branch tag-type detection.
  - Add `detectTagType()` → enum `{TAG_MIFARE_CLASSIC, TAG_NTAG, TAG_UNKNOWN}`.
  - Add `readBambuTag(SpoolData& out)` in rfid_driver using `ntag2xx_ReadPage()`.
  - Map Bambu material codes + color to `SpoolData` fields for unified inventory flow.

---

## 🔭 Future / Non-Blocking

- Printer integration: auto-update remaining filament weight from OctoPrint / Klipper / Creality Cloud
- Multi-printer support (track which spool is in which printer)
- Statistics dashboard: consumption trends from SD card usage logs
- Export/import inventory via USB or WiFi download
- NeoPixel RGB status LED option (single-pin, replaces separate red/green LEDs)
- WiFi portal close harmless error (`WebServer.cpp:638`) — investigate or suppress

---

*Last updated: 2026-03-20*
