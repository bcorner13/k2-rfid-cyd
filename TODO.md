# K2 RFID Tag Programmer — TODO

---

## 🎯 Current Focus

**Milestone 1: Hardware-Stable Core** — Fix the 9 hardware-observed bugs (collected 2026-03-10) before
advancing features. Goal: RFID read/write works reliably on a real Creality spool, no UI lockouts,
clean boot output.

---

## 🐛 Active Bugs (fix in this order)

Issues collected 2026-03-10 by walking through the physical device. Ordered by impact and dependency.

### Step 1 — Fix Issue #9: PN532 invalid pin log errors (cosmetic, low risk)

**Symptom:** Two `Invalid pin selected` messages at boot from the PN532 constructor.
**Root cause:** `PN532_IRQ` and `PN532_RESET` are defined as `(-1)` (int). `Adafruit_PN532` constructor
takes `uint8_t`, so -1 silently becomes 255 (0xFF). Arduino HAL's `__pinMode(255)` logs the error.
RFID still functions — cosmetic only.
**Fix:** Change defines to `(0xFF)` (the documented "no pin" sentinel for this library), or guard
`__pinMode` calls with a valid-pin check.

---

### Step 2 — Fix Issues #3 & #7: State machine UI lockout (no ERROR→IDLE recovery)

**Symptom #3:** Any RFID auth failure → `State: -> ERROR` → all buttons ghosted except Settings→About.
Only fix is reboot.
**Symptom #7:** Settings → Update Database → `State: -> UPDATING DB` → `State: -> ERROR` → same total
UI lockout.
**Root cause:** Both paths share the same bug: the `ERROR` state in the state machine has no exit
transition back to `IDLE`. `updateButtonStates()` disables all action buttons in ERROR state with no
dismiss / retry path wired to the UI.
**Fix:** (a) Add `ERROR → IDLE` transition triggered by a dismiss tap or a timeout. (b) Wire a visible
"Dismiss" / "Retry" button or status-bar tap to fire it. (c) Ensure `updateButtonStates()` re-enables
controls once back in IDLE.

---

### Step 3 — Fix Issue #2: RFID Key A authentication failure on real Creality spool

**Symptom:** Auto-read detects tag `F5:A7:68:19` but fails: `Auth failed for sector 1`.
**Root cause (suspected):** `generateKeyA()` passes the raw UID buffer (4–7 bytes) directly to
`mbedtls_aes_crypt_ecb()` which requires a full 16-byte input block. The remaining bytes are whatever
is on the stack — a buffer overread producing a wrong key, hence auth failure.
**Fix:** Zero-pad the UID into a 16-byte array before calling AES-ECB. Verify the derivation scheme
against the Creality K2 Plus RFID spec (`docs/rfid/creality-k2plus-rfid-spec.md`) to confirm the
correct key input format (some implementations XOR or hash additional fields before AES).

---

### Step 4 — Fix Issue #1: Splash screen WiFi status not displayed

**Symptom:** Splash shows but WiFi connection status label is blank / not updating. No "Continue"
button visible. Feature was working in the Gemini session that created it.
**Likely cause:** Event callback or label update path broken during the board migration merge; status
labels may not have been wired to the WiFi state callbacks, or the 10-second hold timer fires before
the labels populate.
**Fix:** Trace WiFi status → splash label update path; confirm `screen_wifi` / splash status labels
are populated on `NETWORK_CONNECTED` / `NETWORK_FAILED` events; confirm Continue button is shown and
tap navigates to main screen.

---

### Step 5 — Fix Issue #8: First-boot .tmp file errors for config.json / inventory.json

**Symptom:** On first boot (no existing files), both `config.json` and `inventory.json` log
"does not exist" errors during `.tmp` recovery.
**Root cause:** `recoverTmpFile()` calls `LittleFS.exists()` on the `.tmp` path — if neither the base
file nor the `.tmp` exists, the error is benign but noisy. The atomic-write pattern assumes at least
one of the two files exists.
**Fix:** Guard `recoverTmpFile()` with an existence check; on first boot, silently skip recovery and
proceed to create the file from defaults. No functional impact after boot; this is a log-cleanliness
fix.

---

### Step 6 — Fix Issue #6: Audio feedback silent on RFID read/write

**Symptom:** RFID read/write completes (or fails) with no audio. Board is V3.1 with onboard speaker.
**Context:** `uiSound.init()` must be called in `setup()` with the correct path for V3.1. V3.1 uses
I2S (same as V2.0 path; build flag `-DBOARD_MATOUCH_V2` enables it). `playClick()` / `playStartup()`
calls may be commented out or the feedback module may call legacy GPIO buzzer functions instead of
`uiSound`.
**Fix:** Confirm `-DBOARD_MATOUCH_V2` in `platformio.ini` for the V3.1 env. Confirm `uiSound.init()`
called in `setup()`. Wire feedback module events (`RFID_READ_OK`, `RFID_READ_FAIL`, etc.) to
`uiSound.playClick()` / tone sequences.
**Future (non-blocking):** Add a distinct ascending scale melody on successful read, descending on
failure, and a random short progression on boot. Log as enhancement once base audio is working.

---

### Step 7 — Fix Issues #4 & #5: About screen live status + investigate low heap

**Issue #4:** About screen shows only static hardware specs. Should show live WiFi SSID/IP and RFID
connection status at minimum.
**Fix:** Add two live-status labels to `ScreenAbout::show()` populated from `network.isConnected()` /
`network.getSSID()` / `network.getIP()` and `rfid.isInitialized()`. Call `show()` each time the
screen is loaded (already the pattern).

**Issue #5:** Heap free reported as ~78KB — suspiciously low for a device with 512KB SRAM and 8MB PSRAM.
**Investigate:** Check whether LVGL draw buffers, ArduinoJson doc, and PSRAM allocations are correctly
routed to PSRAM. Check for heap fragmentation. Log `ESP.getFreeHeap()`, `ESP.getFreePsram()`,
`ESP.getMinFreeHeap()` at startup and after each major init step. 78KB heap may be normal if most
working memory is in PSRAM — need to confirm PSRAM free is healthy first.

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

### Milestone 3 — Inventory Flow
- Scan tag → SpoolRecord auto-added to inventory (or reconciled if UID already exists)
- Library pick → `FilamentProfile` → SpoolRecord → write tag → inventory updated
- Spool Detail screen: weight shown, weight update modal works, history entry logged
- Custom Spool Entry wizard: all 5 steps functional, Save Locally + Save+Write Tag both work
- Inventory screen: list shows all spools with color swatch, weight bar, status dot

### Milestone 4 — Polish & Completeness
- Audio feedback: distinct tones for read-ok / write-ok / error / low-battery; boot melody
- About screen: live WiFi SSID/IP + RFID status (Issue #4)
- Settings screen: brightness slider, beep on/off toggle both functional
- Sleep mode: screen timeout + wake on touch (GT911 interrupt)
- Memory audit: PSRAM usage confirmed healthy (Issue #5), heap budget documented

### Milestone 5 — Production Prep *(stretch / future)*
- Battery monitoring via GPIO6 ADC (voltage-to-SoC table; low-battery state)
- SD card backup: inventory.json + usage log CSV appended on each write
- V3.1 I2S audio path confirmed (currently using V2.0 path as proxy)
- OTA database update from printer HTTP API tested end-to-end
- P2 FSD items: I2C bus recovery (P2.1), string length enforcement (P2.6), memory monitoring (P2.5)

---

## 🔭 Future / Non-Blocking

- Printer integration: auto-update remaining filament weight from OctoPrint / Klipper / Creality Cloud
- Multi-printer support (track which spool is in which printer)
- Statistics dashboard: consumption trends from SD card usage logs
- Export/import inventory via USB or WiFi download
- NeoPixel RGB status LED option (single-pin, replaces separate red/green LEDs)
- WiFi portal close harmless error (`WebServer.cpp:638`) — investigate or suppress

---

*Last updated: 2026-03-10*
