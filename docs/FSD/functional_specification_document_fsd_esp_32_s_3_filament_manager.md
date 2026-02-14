# Functional Specification Document (FSD)

## Project: ESP32-S3 Filament Manager UI

## 1. Purpose
This document defines the functional requirements, architecture, and operational behavior of the ESP32-S3 Filament Manager application. The system provides a touchscreen-based user interface for managing 3D printer filament inventory, including RFID tag scanning, spool cataloging, usage tracking, custom spool creation, and filament profile visualization.

The FSD serves as a stable reference for implementation, debugging, and future enhancements.

---

## 2. System Overview

### 2.1 Hardware Platform
 **Waveshare ESP32-S3 Touch LCD 4.3" (4.3C variant — AI Voice model)**

  **MCU**

  - **ESP32-S3-WROOM-1-N16R8** module — dual-core Xtensa LX7 @ 240 MHz
  - **Flash:** 16 MB (QIO mode)
  - **PSRAM:** 8 MB Octal SPI (BOARD_HAS_PSRAM, `qio_opi` memory type)
  - **SRAM:** 512 KB (internal)

  **Display**

  - **800x480 RGB LCD** — 16-bit parallel bus (RGB565)
  - Driven by **LovyanGFX** via LGFX_Config.h
  - Pixel clock: 12 MHz (lowered from default to reduce jitter)
  - Data pins: D0–D15 on specific GPIOs; HSYNC=GPIO46, VSYNC=GPIO3, PCLK=GPIO7, HENABLE=GPIO5

  **Touch**

  - **GT911** capacitive touch controller
  - I2C address 0x5D on shared bus (GPIO8=SDA, GPIO9=SCL, GPIO4=IRQ)

  **I2C Expander**

  - **CH422G (U10)** — I2C I/O expander on same bus (GPIO8/9)
  - Uses fixed 8-bit command addresses (not standard 7-bit I2C addressing): 0x70 (OC output), 0x38 (push-pull output), 0x48 (set mode), 0x4D (read input)

  **CH422G EXIO Pin Assignments (from schematic):**

  | EXIO Pin | Net Name | Function |
  |----------|----------|----------|
  | EXIO0 | DI0 | Digital input 0 (via optocoupler T5) |
  | EXIO1 | CTP_RST | Touch reset — pulse LOW→HIGH on boot |
  | EXIO2 | DISP | Display enable |
  | EXIO3 | PA_CTRL | NS4150B speaker amplifier enable |
  | EXIO4 | SDCS | SD card chip select |
  | EXIO5 | DI1 | Digital input 1 (via optocoupler T6) |
  | EXIO6 | DOUT0/DO0 | Digital output 0 (via optocoupler T1) |
  | EXIO7 | DOUT1/DO1 | Digital output 1 (via optocoupler T3) |
  | EXIO_PWM | Backlight | LCD backlight brightness (PWM via AP3032 boost driver) |
  | EXIO_ADC | VBAT sense | Battery voltage divider (R18 20K / R19 10K) |

  **Audio (4.3C-specific)**

  The 4.3C is the AI Voice variant and includes a complete audio subsystem. Not currently used by the filament manager, but documented here for completeness.

  - **ES8311** audio codec — I2S DAC (speaker playback) + ADC (line in), I2C address **0x18** (CE=LOW)
  - **ES7210** ADC with echo cancellation — 4-channel mic ADC, I2C address **0x40** (AD0=0, AD1=0)
  - **NS4150B** (U13) — Class D mono power amplifier, enabled via **EXIO3** (CH422G)
  - **Dual MEMS microphones** (MIC1, MIC2) — connected to ES7210 MIC1P/N and MIC2P/N, powered by MICBIAS12
  - **Speaker header** (H4) — 2-pin, driven by NS4150B PA output

  **Audio Pin Mapping (I2S bus):**

  | Signal | GPIO | Direction | Connects To |
  |--------|------|-----------|-------------|
  | I2S_MCLK | IO6 | ESP32 → Codec | ES8311 pin 2 (MCLK), ES7210 pin 5 (MCLK) |
  | I2S_SCLK | IO44 | ESP32 → Codec | ES8311 pin 6 (SCLK), ES7210 pin 9 (SCLK) |
  | I2S_LRCK | IO16 | ESP32 → Codec | ES8311 pin 8 (LRCK), ES7210 pin 10 (LRCK) |
  | I2S_DSDIN | IO15 | ESP32 → ES8311 | ES8311 pin 9 (DAC data input for playback) |
  | I2S_ASDOUT | IO43 | Codec → ESP32 | ES8311 pin 7 + ES7210 pin 11 via R70 51Ω (ADC capture data) |
  | PA_CTRL | EXIO3 | CH422G → NS4150B | U13 pin 1 (amplifier enable) |

  Audio I2C control shares the main I2C bus (IO8=SDA, IO9=SCL). Note: IO6 is shared with the display data bus (I2S_MCLK), requiring careful bus arbitration if both audio and display are active simultaneously.

  **RTC**

  - **PCF85063ATL (U3)** — real-time clock on shared I2C bus, address **0x51**
  - 32.768 kHz crystal (Y1)
  - Interrupt output (RTC_INT) → **EXIO_RESET** on CH422G (active-low)
  - Battery-backed via VBAT through Schottky diodes (D15, D16)
  - Provides timestamps for inventory weight history and spool creation dates

  **SD Card**

  - Micro SD card slot (SD1) — SPI interface, FAT32 formatted
  - **IO11** = MOSI, **IO12** = SCK, **IO13** = MISO
  - Chip select: **EXIO4** (via CH422G, accent through R105 0R to IO10)
  - 128 GB card installed; used for database backups, usage history logs, and data export (see Section 5.7)

  **RFID**

  - **PN532 NFC Module V3** (13.56 MHz) — MIFARE Classic 1K tags
  - Connected via **I2C** on the shared bus (IO8=SDA, IO9=SCL), I2C address **0x24**
  - Powered from the board's I2C header (VCC, GND, SDA, SCL)
  - Supports standard CFS v1 tags and extended v2 tags (see Section 7)

  **Digital I/O (optocoupler-isolated) — P1 Header**

  - **DIN0** (EXIO0): Digital input via optocoupler T5 (PC814), active through R78 3.6K
  - **DIN1** (EXIO5): Digital input via optocoupler T6 (PC814), active through R73 3.6K
  - **DOUT0** (EXIO6): Digital output via optocoupler T1 (PC817), driven through R57 510Ω
  - **DOUT1** (EXIO7): Digital output via optocoupler T3 (PC817), driven through R74 510Ω
  - External I/O exposed on header P1 (bottom edge of board)
  - Used for status feedback LEDs and buzzer (see Section 2.1 Feedback Hardware below)

  **Free GPIO Pins (I2S, unused)**

  The following I2S pins are available for general-purpose use since the audio codec is not initialized:

  | GPIO | Original I2S Function | Available For | Notes |
  |------|-----------------------|---------------|-------|
  | IO15 | I2S_DSDIN (DAC data to ES8311) | General output | Connects to ES8311 pin 9 (high-Z when codec not initialized) |
  | IO16 | I2S_LRCK (L/R clock) | General output | Connects to ES8311 pin 8 + ES7210 pin 10 |

  > **Note:** IO43 and IO44 (I2S_ASDOUT / I2S_SCLK) are NOT available — they are used by UART0 (Serial TX/RX) for upload and monitoring. IO6 (I2S_MCLK) is routed near the RGB display data bus and should not be repurposed.

  **Feedback Hardware**

  External indicators for RFID operation status feedback:

  - **Buzzer:** YMD-12095 active piezo buzzer (5V DC, continuous tone). Active buzzer has a built-in oscillator — apply voltage to sound, remove to silence. No PWM or tone generation needed, just digital HIGH/LOW.
  - **Red LED:** Standard 5mm red LED for failure/error indication
  - **Green LED:** Standard 5mm green LED for success indication

  **Wiring — Option A (recommended): P1 header digital outputs**

  Uses the optocoupler-isolated outputs on the P1 header. The optocouplers switch an external 5V supply, which correctly drives the 5V buzzer and provides clean LED power. All controlled via CH422G EXIO pins over I2C.

  | Device | P1 Output | CH422G Pin | Wiring |
  |--------|-----------|------------|--------|
  | Buzzer (YMD-12095) | DOUT0 | EXIO6 | P1 DOUT0 → buzzer (+), P1 GND → buzzer (−). External 5V on P1 VCC. |
  | Red LED | DOUT1 | EXIO7 | P1 DOUT1 → 220Ω resistor → red LED anode → P1 GND |
  | Green LED | — | — | See note below |

  > **Note on 3 outputs vs 2 DOUT pins:** The P1 header has only 2 digital outputs (DOUT0, DOUT1). For 3 devices (buzzer + 2 LEDs), options include:
  > - Use DOUT0 for buzzer, DOUT1 for a **bicolor red/green LED** (single package, common cathode — color selected by polarity, but only one color at a time since it's one output)
  > - Use DOUT0 for buzzer, DOUT1 for red LED, and **IO15 or IO16** (3.3V GPIO) for green LED (LEDs work fine at 3.3V)
  > - Use a **single RGB/NeoPixel (WS2812) LED** on IO15 or IO16 — one pin, any color (requires NeoPixel library, ~800 bytes RAM)

  **Wiring — Option B: Direct GPIO (IO15 / IO16)**

  Simpler wiring but limited to 3.3V output. LEDs work fine at 3.3V with appropriate resistor. The 5V active buzzer may not trigger reliably at 3.3V — test before committing to this approach.

  | Device | GPIO | Wiring |
  |--------|------|--------|
  | Green LED | IO15 | IO15 → 220Ω → green LED → GND |
  | Red LED | IO16 | IO16 → 220Ω → red LED → GND |
  | Buzzer | DOUT0 (EXIO6) | Via P1 header with external 5V (buzzer needs 5V) |

  **Battery & Power**

  - **CS8501 (U4)** — LiPo charger + DC-DC boost converter (charges single-cell LiPo/18650 via USB and boosts to 5V for system power)
  - **BAT1** — battery connector for 3.7V LiPo / 18650 cell
  - Battery voltage monitored via **EXIO_ADC** (voltage divider R18 20K / R19 10K → ~2.8V at full charge for ADC-safe reading)
  - **SW1** — physical battery on/off switch
  - **SY8293FCC (U1)** — main 5V buck regulator from VIN
  - **TMI3112H (U8)** — 3.3V buck regulator from 5V
  - Board draws ~550mA for display alone; total system draw estimated ~700-800mA (display + ESP32 active + PN532)
  - Battery charging occurs via USB-C when connected; CS8501 handles charge/discharge management natively
  - **Note:** An external TP4056 charge/discharge step-up module (J5019) was evaluated but is **not required** — the on-board CS8501 provides equivalent functionality (LiPo charging + boost conversion). The external module would only be needed if a separate micro-USB charging input is desired for enclosure routing purposes.

  **I2C Bus Summary (IO8=SDA, IO9=SCL)**

  All I2C peripherals share a single bus with 4.7K pull-ups (R100, R101) to I2C_VCC:

  | Device | I2C Address | Function |
  |--------|-------------|----------|
  | GT911 | 0x5D | Capacitive touch controller |
  | CH422G (U10) | Fixed command bytes (0x70, 0x48, 0x4D, etc.) | I/O expander |
  | PCF85063A (U3) | 0x51 | Real-time clock |
  | ES8311 (U11) | 0x18 | Audio codec (DAC/ADC) |
  | ES7210 (U12) | 0x40 | 4-ch mic ADC with echo cancellation |
  | PN532 (external) | 0x24 | NFC/RFID reader/writer |

  > **Note:** The CH422G uses non-standard fixed command-byte addressing (8-bit: 0x48 for set-mode, 0x70 for OC write, 0x4D for read). The 7-bit equivalent of 0x48 is 0x24, which overlaps with the PN532 default address. In practice this has not caused bus conflicts because the CH422G command protocol differs from standard I2C register access, but if issues arise, the PN532 V3 module supports SPI mode as an alternative.

  **USB**

  - Two USB-C ports: **USB-JTAG** and **UART**
  - UART port used for upload/monitor (more stable)
  - `ARDUINO_USB_CDC_ON_BOOT=0` (CDC disabled, using hardware UART)

  **Connectivity**

  - WiFi (via WiFiManager, currently disabled in code)
  - BLE (available but unused)

  **Board Definition**

  - Custom PlatformIO board: boards/waveshare_s3_43.json
  - Hardware docs: docs/board-variant-4.3C.md

### 2.2 Software Stack
- **Framework:** Arduino (ESP32 core)
- **Graphics:** LVGL 9.x
- **Display Driver:** LovyanGFX
- **Filesystem:** LittleFS (internal flash) + FAT32 (SD card)
- **Data Format:** JSON (gzip-compressed for large files)
- **JSON Parsing:** ArduinoJson v7 (PSRAM-backed allocator)
- **Networking:** WiFiManager (captive portal), HTTPClient (printer API)

---

## 3. High-Level Architecture

### 3.1 Module Breakdown

| Module | Responsibility |
|------|---------------|
| `lvgl_display` | Display init, LVGL tick, splash screen |
| `ui_manager` | Screen transitions, event routing, navigation between all screens |
| `screen_main` | Main/dashboard screen with read/write controls |
| `screen_inventory` | Inventory list/grid UI with scan, search, and filter |
| `screen_spool_detail` | View/edit individual spool: update weight, view history, write to tag |
| `screen_custom_entry` | Multi-step manual entry form for untagged spools |
| `screen_library` | Filament library grid UI; also serves as reference catalog for custom spool creation |
| `screen_filament_select` | Active filament selection |
| `screen_settings` | Application settings |
| `screen_about` | System info / credits |
| `filament_db` | Load, parse, cache filament database from `/material_database.json`; provides reference profiles for custom spool creation |
| `inventory_manager` | Spool inventory CRUD, persistence to `/inventory.json`, usage/weight tracking |
| `network_manager` | Wi-Fi connectivity, HTTP client for printer API (database download) |
| `sd_manager` | SD card mount/unmount, backup writes, usage log appends, export |
| `feedback` | Buzzer and LED control for RFID operation status (beep patterns, red/green LED) via CH422G and/or GPIO |
| `rfid_driver` | PN532 driver; Key A derivation; full read/write for CFS v1 and extended v2 tags |
| `system_state` | Global state machine with states for inventory, editing, and custom entry operations |
| `config_manager` | Persistent configuration via `/config.json` |

---

## 4. Application Startup Flow

1. Boot ESP32-S3
2. Initialize Serial logging
3. Initialize display + LVGL
4. Show splash screen
5. Initialize subsystems:
   - LittleFS mount
   - SD card mount (FAT32, SPI via EXIO4 chip select)
   - Config manager (load `/config.json`)
   - Filament database (load `/material_database.json.gz` from LittleFS)
   - Inventory manager (load `/inventory.json`, create if missing)
   - RFID driver (PN532 init)
   - Network (WiFi, if enabled)
6. Display subsystem status on splash screen
7. Transition to inventory screen (default home screen)
8. Initialize UI screens

---

## 5. Filament Database

### 5.1 Storage
- **Active copy:** `/material_database.json.gz` on LittleFS (gzip-compressed, ~40 KB)
- **Backup copy:** `/material_database.json` on SD card (uncompressed, ~180 KB)
- At boot, the active gzipped copy on LittleFS is decompressed into PSRAM for parsing
- The SD card backup preserves the raw JSON as downloaded from the printer

### 5.2 JSON Structure (Expected)
```json
{
  "result": {
    "list": [
      {
        "base": {
          "id": "...",
          "brand": "...",
          "name": "...",
          "meterialType": "...",
          "colors": ["#RRGGBB"]
        },
        "kvParam": {
          "nozzle_temperature": 200,
          "nozzle_temperature_min": 190,
          "nozzle_temperature_max": 220,
          "hot_plate_temp": 60,
          "hot_plate_temp_min": 50,
          "hot_plate_temp_max": 70,
          "print_speed_min": 30,
          "print_speed_max": 600,
          "fan_speed_percent": 100,
          "diameter": 1.75,
          "density": 1.24
        }
      }
    ]
  }
}
```

### 5.3 FilamentProfile Structure

```cpp
struct FilamentProfile {
    // Identity
    String id;              // Upstream database ID
    String brand;           // e.g., "Hyper", "Creality"
    String name;            // e.g., "PLA Matte Blue"
    String material_type;   // e.g., "PLA", "PETG", "TPU"

    // Color
    uint32_t color_hex;     // 0xRRGGBB
    String color_name;      // Human-readable color name

    // Temperature settings
    uint16_t nozzle_temp;       // Default nozzle temperature (°C)
    uint16_t nozzle_temp_min;   // Min nozzle temperature (°C), e.g., 190
    uint16_t nozzle_temp_max;   // Max nozzle temperature (°C), e.g., 220
    uint16_t bed_temp;          // Default bed temperature (°C)
    uint16_t bed_temp_min;      // Min bed temperature (°C), e.g., 50
    uint16_t bed_temp_max;      // Max bed temperature (°C), e.g., 70

    // Print settings
    uint16_t print_speed_min;   // Min print speed (mm/s), e.g., 30
    uint16_t print_speed_max;   // Max print speed (mm/s), e.g., 600
    uint8_t  fan_percent;       // Part cooling fan (0-100%), e.g., 100

    // Physical properties
    uint16_t diameter_um;       // Filament diameter in microns, e.g., 1750 (1.75mm)
    float    density;           // Material density (g/cm³), e.g., 1.24

    // Spool
    uint32_t weight_g = 1000;   // Net weight of filament (g)

    // Origin
    bool is_custom = false;     // true if user-created profile
};
```

Fields populated from the Creality upstream database during load (the JSON already contains minTemp/maxTemp, density, diameter, fan speed data). Custom profiles created by the user set `is_custom = true`.

### 5.4 FilamentDB Responsibilities
- Mount LittleFS
- Load JSON into PSRAM-backed `JsonDocument`
- Parse and validate structure
- Populate in-memory cache (`std::vector<FilamentProfile>`)
- Expose read-only accessors
- Serve as reference catalog when creating custom spools

### 5.5 Memory Strategy
- Gzipped JSON decompressed into PSRAM-backed `JsonDocument`
- Parsed data copied into compact `std::vector<FilamentProfile>` cache
- JSON document discarded after load
- Runtime cache resides in RAM; source files remain on LittleFS (gzipped) and SD card (raw)

### 5.6 Database Update Mechanism

The filament database is updated manually via the Settings screen "Update Database" button. The system fetches the latest material database directly from a Creality K2 Plus printer on the local network.

**Printer API endpoint:**
```
GET http://{printer_ip}/downloads/defData/material_database.json
```

**Printer discovery (optional verification):**
```
GET http://{printer_ip}/info
→ Response JSON contains "model" field ("F008" = K2 Plus, "F018" = Hi)
```

**Update flow:**

1. User taps "Update Database" in Settings
2. Text input shows saved printer IP (from `config.json`), user confirms or edits
3. "Test Connection" verifies printer is reachable via `GET /info`
4. On success, `GET /downloads/defData/material_database.json` downloads the full JSON
5. Raw JSON saved to SD card: `/material_database.json` (backup, overwrites previous)
6. JSON gzip-compressed in memory, written to LittleFS: `/material_database.json.gz`
7. FilamentDB cache reloaded from the new data
8. UI displays: success status, profile count, timestamp (from RTC)
9. Printer IP saved to `config.json` for next time

**Error handling:**
- Network unreachable → "Cannot reach printer at {ip}" with retry option
- Invalid response (not JSON, wrong format) → "Invalid database format" — previous DB preserved
- LittleFS write failure → "Storage error" — SD backup still available for manual recovery
- Download interrupted → previous database remains active (atomic replace: write to temp file, rename)

### 5.7 Storage Architecture

The system uses a dual-storage approach: LittleFS on internal flash for active runtime data, SD card for backups and unbounded history.

**LittleFS (internal flash) — active runtime data:**

| File | Size | Purpose |
|------|------|---------|
| `/material_database.json.gz` | ~40 KB | Active filament database (gzip-compressed) |
| `/inventory.json` | ~50 KB | Active spool inventory |
| `/config.json` | <1 KB | App configuration (printer IP, brightness, beep, WiFi) |

**SD Card (FAT32, 128 GB) — backups and history:**

| File/Directory | Purpose |
|----------------|---------|
| `/material_database.json` | Uncompressed backup of last downloaded DB |
| `/backups/inventory_YYYYMMDD_HHMMSS.json` | Timestamped inventory backups (created before each DB update) |
| `/logs/usage_log.csv` | Full weight history log (spool_id, weight_g, timestamp) — unbounded |
| `/exports/` | User-initiated data exports |

**Benefits of this split:**
- LittleFS budget stays small (~100 KB total) — no flash wear concerns
- SD card handles unbounded data (full usage history, multiple backups) without memory caps
- Weight history in `inventory.json` can be kept small (last 10 entries per spool) since the full log lives on SD
- Raw JSON backup on SD enables manual recovery if LittleFS gets corrupted

### 5.8 FilamentProfile to SpoolData Mapping

When a filament is selected from the library, `FilamentProfile` fields map to `SpoolData` as follows:

| FilamentProfile | SpoolData | On RFID Tag (v1) | On RFID Tag (v2 extended) |
|-----------------|-----------|-------------------|---------------------------|
| `.brand` | `._brandName` | Not stored | Sector 12 (12 chars) |
| `.name` | `._displayName` | Not stored | Sector 13 (16 chars) |
| `.material_type` | `._materialType` | 5 chars, space-padded | 5 chars, space-padded |
| `.color_hex` / `.color_name` | `._materialColorNumeric` / `._materialColorString` | 6 hex chars | 6 hex chars |
| `.weight_g` | `._materialWeight` | 4-digit length (`len = 330 * weight / 1000`) | 4-digit length |
| `.nozzle_temp` / `.bed_temp` | Not in SpoolData | Not stored | Sector 10 (temp ranges) |
| `.nozzle_temp_min/max` | Not in SpoolData | Not stored | Sector 10 |
| `.bed_temp_min/max` | Not in SpoolData | Not stored | Sector 10 |
| `.print_speed_min/max` | Not in SpoolData | Not stored | Sector 11 |
| `.fan_percent` | Not in SpoolData | Not stored | Sector 11 |
| `.diameter_um` | Not in SpoolData | Not stored | Sector 12 |
| `.density` | Not in SpoolData | Not stored | Sector 12 |

---

## 6. Inventory Database

### 6.1 Purpose

The inventory system tracks all owned filament spools. Spools can be added by scanning RFID tags, selecting from the filament library, or manual entry. Each spool record stores its filament properties inline (not as a reference), so inventory data survives database updates.

### 6.2 Storage
- Location: `/inventory.json` on LittleFS
- Managed by `InventoryManager` (`src/inventory_manager.cpp`)
- Estimated size: ~500 bytes per spool × 100 spools max ≈ 50 KB

### 6.3 JSON Schema (`/inventory.json`)

```json
{
  "version": 1,
  "spools": [
    {
      "spool_id": "SPL-0001",
      "tag_uid": "04:A3:2B:1C:7D:80:00",
      "profile": {
        "brand": "Hyper",
        "name": "PETG Black",
        "material_type": "PETG",
        "color_hex": "000000",
        "diameter_um": 1760,
        "nozzle_temp_min": 220,
        "nozzle_temp_max": 260,
        "bed_temp_min": 60,
        "bed_temp_max": 80,
        "print_speed_min": 30,
        "print_speed_max": 600,
        "fan_percent": 50,
        "density": 1.27
      },
      "initial_weight_g": 1000,
      "current_weight_g": 750,
      "status": "active",
      "source": "manual",
      "created_at": 1700000000,
      "updated_at": 1700100000,
      "weight_history": [
        {"weight_g": 1000, "timestamp": 1700000000},
        {"weight_g": 750, "timestamp": 1700100000}
      ]
    }
  ],
  "next_id": 2
}
```

### 6.4 Key Fields

| Field | Type | Description |
|-------|------|-------------|
| `spool_id` | string | Auto-generated unique ID (`SPL-NNNN`) |
| `tag_uid` | string | RFID tag UID if tagged; empty string if local-only |
| `profile` | object | Inline filament properties (brand, material, color, diameter, temps, speeds, fan, density) |
| `initial_weight_g` | uint32 | Weight when first added to inventory |
| `current_weight_g` | uint32 | Last known remaining weight |
| `status` | enum | `"active"`, `"empty"`, `"archived"` |
| `source` | enum | `"scan"` (from RFID tag), `"library"` (from DB selection), `"manual"` (custom entry) |
| `created_at` | uint32 | Unix timestamp of creation |
| `updated_at` | uint32 | Unix timestamp of last modification |
| `weight_history` | array | Timestamped weight entries for tracking consumption over time |

### 6.5 InventoryManager API

```
init()                              — Load /inventory.json; create file with empty schema if missing
save()                              — Persist full inventory to LittleFS (overwrite)
addSpool(profile, weight, uid)      — Create new inventory record; returns spool_id
removeSpool(spool_id)               — Set status to "archived" (soft delete)
updateWeight(spool_id, weight)      — Update current_weight_g; append to weight_history
getSpoolByUID(tag_uid)              — Lookup spool by RFID tag UID; returns nullptr if not found
getSpoolById(spool_id)              — Lookup spool by internal ID
getAllActive()                       — Return vector of active (non-archived) spools
getSpoolCount()                     — Total number of active spools
```

### 6.6 Memory Strategy
- Full inventory JSON loaded into PSRAM during `init()`
- Parsed into `std::vector<SpoolRecord>` cache in RAM
- JSON document discarded after load
- `save()` serializes cache back to JSON and overwrites `/inventory.json`
- 100 spool records ≈ 50 KB on LittleFS, ~20 KB in RAM cache
- Weight history capped per spool (e.g., last 50 entries) to bound memory growth

---

## 7. CFS RFID Tag Data Format

### 7.1 Tag Type
MIFARE Classic 1K — 1024 bytes, 16 sectors, 4 blocks/sector, 16 bytes/block.

### 7.2 SpoolData String Format (Tag Payload)

Fixed-length ASCII string (34+ characters), uppercased, written to tag data sectors:

| Position | Length | Field | Example |
|----------|--------|-------|---------|
| 0-4 | 5 | Date code | `AB124` |
| 5-8 | 4 | Vendor ID | `0276` (Creality) |
| 9-10 | 2 | Batch code | `A2` |
| 11 | 1 | Separator | `1` |
| 12-16 | 5 | Material type (space-padded) | `PLA  ` or `PETG ` |
| 17 | 1 | Color prefix | `0` |
| 18-23 | 6 | Color hex RGB | `FFFFFF` |
| 24-27 | 4 | Material length in mm (zero-padded) | `0330` |
| 28-33 | 6 | Serial number (random) | `123456` |
| 34+ | var | Reserve | `000000` |

**Weight ↔ Length conversion:** `length_mm = 330 * weight_g / 1000`

**On-tag limitations (v1):**
- Material type: max 5 chars (e.g., "PLA-Silk" truncated to "PLA-S")
- Brand name: not stored (lost on tag read)
- Filament name: not stored (lost on tag read)

### 7.3 Sector Layout (Standard CFS v1)

| Sector | Purpose | Mutability |
|--------|---------|------------|
| 0 | Manufacturer / UID | Immutable |
| 1 | Tag format & version (magic: `K2PF`, version byte) | Immutable |
| 2 | Filament identity (vendor product ID, material enum, diameter) | Immutable |
| 3 | Material & color (RGB + name) | Immutable |
| 4 | Vendor metadata | Immutable |
| 5 | Spool initialization (initial length/weight) | Write-once |
| 6-8 | Remaining filament (3 mirrored copies) | Mutable |
| 9 | Usage counters (consumed length/weight) | Mutable |
| 10-13 | Reserved (used by extended v2 format — see Section 7.5) | See below |
| 14 | Reserved | Must not modify |
| 15 | CRC32 checksum | Mutable |

### 7.4 Authentication
- Key A derived from tag UID
- Key B valid but unused
- Sector trailers must not be modified except during controlled reinitialization

### 7.5 Extended Tag Format (v2)

Backward-compatible extension using reserved sectors 10-13. Existing CFS v1 tags continue to work unmodified — the system detects extended data via the version byte in sector 1.

**Version Detection:** Sector 1, Block 4, offset 0x04:
- `0x01` = Standard CFS tag (Creality original, v1)
- `0x02` = Extended tag (includes print settings in sectors 10-13)

When reading a tag, the system checks the version byte. If `0x01`, only sectors 1-9 are read. If `0x02`, sectors 10-13 are also read to populate extended fields (brand, name, temp ranges, speed, fan, diameter, density).

#### Sector 10, Block 40: Print Temperature Settings

| Offset | Size | Field | Example |
|--------|------|-------|---------|
| 0x00 | 2 | Nozzle temp min (°C) | 220 |
| 0x02 | 2 | Nozzle temp max (°C) | 260 |
| 0x04 | 2 | Bed temp min (°C) | 60 |
| 0x06 | 2 | Bed temp max (°C) | 80 |
| 0x08 | 2 | Nozzle temp default (°C) | 240 |
| 0x0A | 2 | Bed temp default (°C) | 70 |
| 0x0C | 4 | Reserved | 0x00000000 |

#### Sector 11, Block 44: Print Speed & Fan Settings

| Offset | Size | Field | Example |
|--------|------|-------|---------|
| 0x00 | 2 | Print speed min (mm/s) | 30 |
| 0x02 | 2 | Print speed max (mm/s) | 600 |
| 0x04 | 1 | Fan percent (0-100) | 50 |
| 0x05 | 1 | Reserved | 0x00 |
| 0x06 | 2 | Max volumetric flow (mm³/s × 10) | 240 (= 24.0 mm³/s) |
| 0x08 | 8 | Reserved | 0x00... |

#### Sector 12, Block 48: Physical Properties

| Offset | Size | Field | Example |
|--------|------|-------|---------|
| 0x00 | 2 | Diameter (µm) | 1760 (= 1.76mm) |
| 0x02 | 2 | Density (× 100) | 127 (= 1.27 g/cm³) |
| 0x04 | 12 | Brand name (ASCII, null-padded) | `"Hyper\0\0\0\0\0\0\0"` |

#### Sector 13, Block 52: Extended Name

| Offset | Size | Field | Example |
|--------|------|-------|---------|
| 0x00 | 16 | Product name (ASCII, null-padded) | `"PETG Black\0\0\0\0\0\0"` |

**Encoding notes:**
- All multi-byte integer values are little-endian
- All strings are ASCII, null-padded to fill the field
- CRC32 in sector 15 is recalculated to cover sectors 1-13 (extended from 1-9 for v1)

### 7.6 Write Semantics
All writes are transactional: authenticate → read existing → validate → write → read back → byte-compare → update CRC → confirm. Partial writes are forbidden. All three mirrors (sectors 6-8) must be updated consistently. For v2 tags, sectors 10-13 are also written atomically with the CRC update.

Full specification: `docs/rfid/creality-k2plus-rfid-spec.md`

---

## 8. App Configuration

### 8.1 Storage
- Location: `/config.json` on LittleFS
- Managed by `ConfigManager` (`src/config_manager.cpp`)

### 8.2 Fields

| Field | Type | Description |
|-------|------|-------------|
| `beep_enabled` | bool | Audible feedback on read/write |
| `brightness` | int | Display brightness level |
| `wifi_ssid` | string | Stored WiFi network |
| `wifi_password` | string | Stored WiFi password |
| `printer_ip` | string | Last-used printer IP for database updates (e.g., `"192.168.1.100"`) |
| `db_updated_at` | uint32 | Unix timestamp of last successful database update |
| `db_profile_count` | uint16 | Number of profiles in current database |

### 8.3 Access
- `config.init()` — loads from LittleFS
- `config.save()` — persists to LittleFS
- `config.data.beep_enabled` — direct field access

---

## 9. Feedback (Buzzer & LEDs)

### 9.1 Purpose

Provide immediate physical feedback when RFID operations complete. A buzzer sounds for audible confirmation, and LEDs provide visual status (green = success, red = failure). Feedback is independent of the on-screen status text and works even if the user isn't looking at the display.

### 9.2 Hardware

- **Buzzer:** YMD-12095 active piezo buzzer (5V DC, continuous tone, ~85dB). Active type — built-in oscillator, no PWM needed. Apply voltage = beep, remove = silence.
- **Green LED:** 5mm green LED (success indicator)
- **Red LED:** 5mm red LED (failure/error indicator)

Wiring depends on chosen option (see Section 2.1 Feedback Hardware). Recommended: buzzer on DOUT0 (EXIO6), LEDs on IO15/IO16 or DOUT1 (EXIO7).

### 9.3 Feedback Module (`src/feedback.cpp`)

```cpp
// Global instance
extern Feedback feedback;

class Feedback {
public:
    void init();                    // Configure GPIO pins / CH422G outputs
    void success();                 // Green LED ON, beep short, then LEDs off after delay
    void failure();                 // Red LED ON, beep long, then LEDs off after delay
    void beep(uint16_t duration_ms); // Buzzer on for duration (non-blocking via timer)
    void ledGreen(bool on);         // Green LED on/off
    void ledRed(bool on);           // Red LED on/off
    void allOff();                  // All outputs off
    void update();                  // Called from loop() — handles timed LED/buzzer off
};
```

### 9.4 Feedback Patterns

| Event | Buzzer | Green LED | Red LED | Duration |
|-------|--------|-----------|---------|----------|
| RFID read success | 1 short beep (100ms) | ON 1s | OFF | 1 second total |
| RFID write success | 2 short beeps (100ms on, 100ms off, 100ms on) | ON 1.5s | OFF | 1.5 seconds |
| RFID read/write failure | 1 long beep (500ms) | OFF | ON 2s | 2 seconds |
| Tag detected (scan) | 1 short beep (50ms) | Flash 200ms | OFF | 200ms |
| Inventory spool saved | 1 short beep (100ms) | ON 500ms | OFF | 500ms |
| DB update success | 2 short beeps | ON 1s | OFF | 1 second |
| DB update failure | 1 long beep | OFF | ON 2s | 2 seconds |

### 9.5 Configuration

- **`config.data.beep_enabled`** (bool): When `false`, buzzer is silenced but LEDs still operate. This is the "Beep on R/W" toggle in Settings.
- Feedback calls check `beep_enabled` before activating buzzer; LED behavior is always active.

### 9.6 Implementation Notes

- **Non-blocking:** `feedback.success()` and `feedback.failure()` set outputs and record a timestamp. `feedback.update()` (called from `loop()`) turns outputs off after the pattern duration expires. No `delay()` calls.
- **CH422G access:** If using DOUT0/DOUT1, the CH422G expander must remain initialized across the application lifecycle (currently it's a local variable in `init_ch422g_4_3c()`). The expander instance should be promoted to a global or managed by a shared driver.
- **Active buzzer at 5V:** The YMD-12095 is rated for 5V DC. If connected via DOUT0 (optocoupler), the P1 header's external supply provides 5V. If connected directly to an ESP32 GPIO (3.3V), the buzzer may sound at reduced volume or not trigger — test before committing.
- **GPIO LED drive:** ESP32-S3 GPIOs can source ~40mA. A standard 5mm LED with 220Ω resistor draws ~10mA at 3.3V — well within limits.

---

## 10. Filament Library Screen

### 10.1 Purpose
Displays all available filament profiles in a grid layout optimized for a 4.3" touchscreen. Also serves as the reference catalog when creating custom spools — the user can browse and select an existing profile as a starting point for a custom spool.

### 10.2 Layout
- Title bar (top)
- Back button (top-left)
- Scrollable grid (bottom)

### 10.3 Grid Rules
- Fixed column count: **4 columns**
- Rows computed dynamically:
  ```
  rows = ceil(filamentCount / 4)
  ```
- Max rows capped to prevent excessive memory usage

### 10.4 Grid Cell Contents
Each filament cell displays:
- Color swatch
- Brand (top)
- Filament name (bottom)

### 10.5 Behavior
- Grid auto-resizes based on filament count
- Touching a cell selects filament → navigates to main/write screen with selected profile
- Long-press on a cell (future): add directly to inventory from library

---

## 11. UI Manager

### 11.1 Responsibilities
- Screen creation lifecycle
- Screen transitions
- Centralized LVGL event handling
- Navigation state management across all screens

### 11.2 Navigation Map

```
Splash → Inventory (default home screen)

Inventory → Spool Detail          (tap spool row)
Inventory → Custom Entry          (+ Add Custom button)
Inventory → Main/Write Screen     (Scan Tag button → reads tag → write screen)
Inventory → Settings              (settings button)

Spool Detail → Edit Weight        (Update Weight button → number entry)
Spool Detail → Main/Write Screen  (Write to Tag button)
Spool Detail → Inventory          (back)

Custom Entry → Inventory          (save → returns to inventory)
Custom Entry → Main/Write Screen  (save + write tag)

Main → Library                    (select filament)
Main → Inventory                  (back / home)
Main → Settings                   (settings button)

Library → Main                    (select filament → returns to main)
Library → Inventory               (back)

Settings → WiFi Setup              (WiFi Setup button → captive portal flow)
Settings → Update Database         (Update Database button)
Settings → Inventory              (back)
About → Settings                  (back)

Update Database → Settings        (done / back)
WiFi Setup → Settings             (connected / cancelled)
```

### 11.3 Inventory Screen

**Purpose:** Default home screen showing all owned filament spools.

**Layout:**
- Title bar: "Filament Inventory" with spool count
- Action bar: Scan Tag, Add Custom, Settings buttons
- Scrollable list of spool rows

**Spool Row Contents:**
- Color swatch (left)
- Brand + filament name
- Material type badge (e.g., "PLA", "PETG")
- Weight bar: visual indicator of `current_weight_g / initial_weight_g`
- Status badge: active (green), empty (red), archived (gray)

**Behavior:**
- Tap row → navigate to Spool Detail
- Scan Tag → initiate RFID read → if tag found in inventory, show detail; if new, add to inventory
- Add Custom → navigate to Custom Entry screen
- List supports scrolling for large inventories

### 11.4 Spool Detail Screen

**Purpose:** View and edit an individual spool's data.

**Layout:**
- Header: Brand + filament name, color swatch, material type
- Info section: temperature ranges, speed range, fan %, diameter, density
- Weight gauge: circular/bar gauge showing `current_weight_g` vs `initial_weight_g` with percentage
- Weight history: last N entries displayed as a simple list or mini chart
- Action buttons: Update Weight, Write to Tag, Archive/Delete

**Behavior:**
- Update Weight → opens number input dialog; value saved to inventory with timestamp
- Write to Tag → navigates to main/write screen pre-populated with this spool's data
- Archive → sets spool status to "archived", returns to inventory
- Delete → confirmation dialog, then removes from inventory

### 11.5 Custom Entry Screen

**Purpose:** Multi-step form for entering filament data from a spool's label (for untagged spools).

**Steps:**

| Step | Fields | Input Type |
|------|--------|------------|
| 1 | Brand, Name, Material Type | Text input, dropdown for material type |
| 2 | Color, Diameter | Color picker, dropdown (1.75mm / 2.85mm / custom) |
| 3 | Weight (g), Nozzle temp range, Bed temp range | Number input, range sliders |
| 4 | Print speed range, Fan % | Range sliders |
| 5 | Review & Confirm | Summary view |

**Completion Options:**
- **Save locally** → creates inventory record with `source: "manual"`, no RFID tag
- **Save + Write to tag** → creates inventory record, then navigates to write screen to program a blank RFID tag with the spool data (v2 extended format)

**Navigation:**
- Back button on each step returns to previous step
- Cancel returns to inventory without saving
- Previous step data is preserved during navigation

### 11.6 Update Database Screen

**Purpose:** Download the latest filament database from a Creality K2 Plus printer on the local network.

**Layout:**
- Title: "Update Database"
- Printer IP input field (pre-filled from `config.json` if previously saved)
- Current DB info: profile count, last updated timestamp (from RTC)
- Action buttons: Test Connection, Download, Back
- Status/progress area

**Flow:**
1. Screen opens with saved printer IP pre-filled (or empty on first use)
2. User enters/edits printer IP via on-screen keyboard
3. "Test Connection" → `GET http://{ip}/info` → shows printer model or error
4. "Download" → progress indicator → `GET http://{ip}/downloads/defData/material_database.json`
5. On success:
   - Raw JSON saved to SD card (`/material_database.json`)
   - Gzipped copy written to LittleFS (`/material_database.json.gz`)
   - FilamentDB cache reloaded
   - Printer IP and timestamp saved to config
   - Status: "Updated: {count} profiles loaded"
6. On failure: error message with description, previous database preserved

**Requires:** WiFi connected. If WiFi is not configured, prompt user to configure WiFi first in Settings.

### 11.7 Main / Write Screen

**Purpose:** RFID tag read/write dashboard. This is the operational screen for programming and reading CFS tags. Users arrive here from the inventory (Scan Tag / Write to Tag) or the library (select filament).

**Layout (3-region grid, 800x480):**

```
┌──────────────────┬─┬─────────────────────────┐
│                  │ │  Filament Name           │
│   Color Block    │ │  [Brand dropdown    ▼]   │
│   (tap = picker) │ │  [Material dropdown ▼]   │
│   "#RRGGBB"      │ │  ──── Weight ────        │
│                  │ │  [====slider====] 750g   │
├──────────────────┴─┴─────────────────────────┤
│ Ready          [READ] [WRITE] [📋] [⚙]      │
└──────────────────────────────────────────────┘
```

- **Left panel:** Color swatch block (200x160) showing current color with hex/name label. Tap opens color picker modal.
- **Vertical divider:** 4px grey line separating left and right panels.
- **Right panel:** Filament name (read-only label), Brand dropdown (populated from FilamentDB), Material Type dropdown (populated from FilamentDB), weight slider (0-1000g) with numeric label.
- **Bottom bar (100px):** Status label ("Ready" / "Read OK" / "Write failed"), then action buttons: READ, WRITE, Library (list icon), Settings (gear icon).

**Color Picker Modal:**
- Overlay on top layer, 360x340
- 5x5 grid of preset color swatches (from `color_palette.h`)
- Close button (X) top-right
- Tap swatch → updates `currentSpool` color → refreshes dashboard → closes modal

**Behavior:**
- **READ button:** Initiates `rfid.readCFSTag()` → on success, populates dashboard with tag data and shows "Read OK" (green); on failure, shows "No tag / Read failed" (red)
- **WRITE button:** Writes `currentSpool` to tag via `rfid.writeCFSTag()` → "Write OK" (green) or "Write failed" (red)
- **Library button:** Navigates to filament library grid
- **Settings button:** Navigates to settings screen
- **Weight slider:** Real-time update of `currentSpool.weight` as user drags
- **Brand/Type dropdowns:** Currently display-only (set from library selection or tag read); editing updates `currentSpool`

**Pre-population:**
- From Inventory (Write to Tag): spool data fills all fields
- From Library (select filament): profile data fills all fields, weight defaults to profile's `weight_g`
- From Tag Read: tag data fills available fields (brand/name may be empty for v1 tags)
- Default on cold start: "Generic PLA", white, 1000g

### 11.8 Settings Screen

**Purpose:** Application settings and system actions.

**Layout:**
- Title: "Settings" (top center)
- Back button (top left, ← arrow) → returns to Main screen
- Centered container (400x380) with vertically spaced items:

| Item | Type | Description |
|------|------|-------------|
| Beep on R/W | Toggle switch | Enable/disable audible feedback on RFID read/write. Saved to `config.json` immediately on change. |
| WiFi Setup | Button | Launches WiFiManager captive portal for network configuration (see Section 11.9) |
| Update Database | Button | Navigates to Update Database screen (Section 11.6). Requires WiFi to be connected. |
| About | Button | Navigates to About screen (system info, version, credits) |
| Restart Device | Button | Calls `ESP.restart()` — immediate reboot, no confirmation dialog |

**Future additions:**
- Brightness slider (controls backlight via EXIO_PWM)
- Printer IP display/edit (currently only editable via Update Database screen or WiFi captive portal)
- Battery status indicator (when battery operation is implemented)

### 11.9 WiFi Setup

**Purpose:** Configure WiFi credentials for network features (database updates).

**Mechanism:** Uses the **WiFiManager** library, which operates as a captive portal. This is NOT an on-device touchscreen UI — the setup happens on the user's phone or laptop.

**Flow:**
1. User taps "WiFi Setup" in Settings (or "Reset WiFi" in current code)
2. Device creates a WiFi access point: **"K2-RFID-SETUP"** (open, no password)
3. Device LCD shows a status screen: "Connect to WiFi 'K2-RFID-SETUP' on your phone, then open 192.168.4.1"
4. User connects phone/laptop to the "K2-RFID-SETUP" AP
5. Captive portal auto-opens (or user browses to 192.168.4.1)
6. Portal shows:
   - Title: "K2 RFID Tool Setup"
   - WiFi network scan/selection
   - WiFi password entry
   - **Printer IP** custom field (saved to `config.json`)
7. User selects network, enters password, optionally enters printer IP → clicks Save
8. Device connects to the configured WiFi network
9. On success: saves credentials, saves printer IP to `config.json`, returns to Settings screen
10. On failure/timeout: restarts device after 3 seconds

**Important notes:**
- The captive portal is **blocking** — the LVGL UI loop is frozen while the portal runs. The device LCD should show a static message before entering portal mode.
- WiFi credentials are stored by the ESP32 WiFi library in NVS (non-volatile storage), not in `config.json`.
- The printer IP custom parameter is the bridge between WiFi setup and the database update flow.
- First-time setup: no WiFi configured → user must go through this flow before Update Database works.

---

## 12. State Machine

### 12.1 System States

```cpp
enum class SystemState {
    // Core states
    BOOT,                  // System initialization
    IDLE,                  // Waiting for user input (on any screen)

    // RFID operations
    READING_TAG,           // Reading RFID tag data
    WRITING_TAG,           // Writing data to RFID tag
    VERIFYING_TAG,         // Verifying written tag data

    // Inventory operations
    SCANNING_INVENTORY,    // Scanning tag for inventory add
    EDITING_SPOOL,         // Modifying spool data (weight update, etc.)
    CUSTOM_ENTRY,          // Creating custom filament profile

    // Network operations
    UPDATING_DATABASE,     // Downloading filament DB from printer

    // System states
    ERROR,                 // Recoverable error displayed to user
    LOW_BATTERY,           // Battery critical (future, if battery-powered)
    SLEEP                  // Display off, low power (future)
};
```

### 12.2 System Events

```cpp
enum class SystemEvent {
    // Boot
    INIT_DONE,             // All subsystems initialized

    // RFID
    TAG_DETECTED,          // Tag placed on reader
    READ_REQUEST,          // User requests tag read
    WRITE_REQUEST,         // User requests tag write
    OPERATION_SUCCESS,     // Read/write completed successfully
    OPERATION_FAILED,      // Read/write failed

    // Inventory
    SCAN_REQUEST,          // User taps "Scan" in inventory screen
    EDIT_REQUEST,          // User opens spool detail for editing
    CUSTOM_REQUEST,        // User starts custom spool entry
    SAVE_REQUEST,          // User saves spool data (inventory or custom)

    // Network
    DB_UPDATE_REQUEST,     // User initiates database download from printer
    DB_UPDATE_SUCCESS,     // Database download and reload completed
    DB_UPDATE_FAILED,      // Database download or processing failed

    // System
    BATTERY_CRITICAL,      // Battery level critical (future)
    TIMEOUT,               // Operation timeout
    WAKE_UP                // Wake from sleep (future)
};
```

### 12.3 Key Transitions

| From | Event | To |
|------|-------|----|
| BOOT | INIT_DONE | IDLE |
| IDLE | SCAN_REQUEST | SCANNING_INVENTORY |
| IDLE | READ_REQUEST | READING_TAG |
| IDLE | WRITE_REQUEST | WRITING_TAG |
| IDLE | EDIT_REQUEST | EDITING_SPOOL |
| IDLE | CUSTOM_REQUEST | CUSTOM_ENTRY |
| SCANNING_INVENTORY | TAG_DETECTED | READING_TAG |
| READING_TAG | OPERATION_SUCCESS | IDLE |
| READING_TAG | OPERATION_FAILED | ERROR |
| WRITING_TAG | OPERATION_SUCCESS | VERIFYING_TAG |
| WRITING_TAG | OPERATION_FAILED | ERROR |
| VERIFYING_TAG | OPERATION_SUCCESS | IDLE |
| VERIFYING_TAG | OPERATION_FAILED | ERROR |
| EDITING_SPOOL | SAVE_REQUEST | IDLE |
| CUSTOM_ENTRY | SAVE_REQUEST | IDLE |
| IDLE | DB_UPDATE_REQUEST | UPDATING_DATABASE |
| UPDATING_DATABASE | DB_UPDATE_SUCCESS | IDLE |
| UPDATING_DATABASE | DB_UPDATE_FAILED | ERROR |
| ERROR | TIMEOUT | IDLE |

---

## 13. Error Handling & Diagnostics

### 13.1 Filesystem Errors
- Mount failure → splash error
- Missing JSON → splash error (for `material_database.json`); auto-create empty schema (for `inventory.json`)

### 13.2 JSON Errors
- Parse failure → abort load
- Missing keys → logged warning, safe defaults

### 13.3 Memory Safety
- Heap usage logged during DB and inventory load
- PSRAM allocator used for large JSON documents
- Avoid long-lived JSON references
- Inventory weight history capped per spool to bound memory growth

### 13.4 RFID Errors
- Tag not found → user notification, return to idle
- Authentication failure → user notification with error details
- Write verification failure → flag tag as potentially corrupted, do not update CRC
- Extended sector read failure on v1 tag → graceful fallback, use v1 data only

### 13.5 Network / Database Update Errors
- WiFi not connected → prompt user to configure WiFi in Settings before attempting update
- Printer unreachable → "Cannot reach printer at {ip}" with retry option; previous DB preserved
- HTTP error (non-200 response) → display HTTP status code; previous DB preserved
- Invalid response (not JSON or wrong schema) → "Invalid database format"; previous DB preserved
- Download interrupted (connection dropped mid-transfer) → discard partial data; previous DB preserved (atomic replace via temp file + rename)
- LittleFS write failure during gzip save → "Storage error — SD backup available"; raw JSON on SD card remains for manual recovery
- SD card write failure → non-fatal warning (SD backup is optional); LittleFS update proceeds normally

### 13.6 SD Card Errors
- SD card not inserted or mount failure → non-fatal; inventory and DB operate normally from LittleFS only; backup/export features disabled with user notification
- SD card full → warn user; skip backup write; primary LittleFS operations unaffected

---

## 14. Constraints

- Internal SRAM is limited (~512 KB); inventory cache and UI objects must fit alongside LVGL buffers
- PSRAM availability varies by board; JSON parsing should always use PSRAM allocator
- LVGL object creation must occur after LVGL init
- No blocking operations in `loop()`
- Inventory limited to ~100 spools to keep `/inventory.json` within ~50 KB on LittleFS
- LittleFS total budget ~100 KB: `material_database.json.gz` (~40 KB) + `inventory.json` (~50 KB) + `config.json` (<1 KB); see Section 5.7 for full storage architecture
- Weight history per spool capped at last 10 entries in `inventory.json`; full history logged to SD card (`/logs/usage_log.csv`) for unbounded tracking
- SD card is optional — system operates fully from LittleFS alone; SD adds backup, history logs, and export capabilities
- Extended v2 tag sectors (10-13) must not be written to tags that did not originate from this system

---

## 15. Future Extensions

- **Battery operation:** Portable use via 18650 / 3.7V LiPo cell connected to BAT1. The Waveshare 4.3C has full on-board charging and boost circuitry (CS8501). Software support needed:
  - Battery voltage monitoring via EXIO_ADC (CH422G analog input, voltage divider R18/R19)
  - Battery percentage estimation (voltage-to-SoC lookup table for Li-ion discharge curve)
  - Low-battery warning state (`LOW_BATTERY` already defined in state machine)
  - Display brightness auto-dimming to extend runtime
  - Optional sleep mode with wake-on-touch (GT911 interrupt on GPIO4)
  - Estimated runtime: ~2-3 hours with 3000mAh 18650 at ~700-800mA draw (needs real-world testing)
- **Printer integration:** Automatic usage tracking via printer API (OctoPrint, Klipper, Creality Cloud) — architecture supports this via `updateWeight()` API
- **Network sync:** Sync inventory across devices via WiFi
- **Barcode scanning:** Camera-based barcode/QR code scanning for spool identification
- **Material filtering and search:** Filter inventory by material type, color, brand, status
- **Statistics dashboard:** Consumption trends, cost tracking, spool lifetime analytics from SD card usage logs
- **Export/import:** Export inventory and usage history from SD card via USB or WiFi download
- **Multi-printer support:** Track which spool is loaded in which printer; store multiple printer IPs
- **Auto printer discovery:** Subnet scan to find Creality printers automatically (currently manual IP entry)
- **Rich audio feedback:** Leverage ES8311/NS4150B audio subsystem for distinct tones, melodies, or voice prompts (current buzzer provides basic beep patterns — see Section 9)

*Note: Filament database updates via printer HTTP API are implemented in current scope (see Section 5.6).*

---

## 16. Non-Goals (Current Phase)

- Printer motion control
- G-code generation
- Real-time printer telemetry
- Cloud account management
- Multi-user access control

---

## 17. Status

**Document Status:** Draft (v2.1)

This FSD reflects the planned architecture for the filament inventory management system, extending the original RFID read/write tool with spool cataloging, usage tracking, custom spool creation, dual-storage architecture (LittleFS + SD card), and manual filament database updates from Creality printers via HTTP.
