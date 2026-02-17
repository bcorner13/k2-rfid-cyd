# Functional Specification Document (FSD)

## Project: ESP32-S3 Filament Manager UI

## 1. Purpose
This document defines the functional requirements, architecture, and operational behavior of the ESP32-S3 Filament Manager application. The system provides a touchscreen-based user interface for managing 3D printer filament inventory, including RFID tag scanning, spool cataloging, usage tracking, custom spool creation, and filament profile visualization.

The FSD serves as a stable reference for implementation, debugging, and future enhancements.

---

## 2. System Overview

### 2.1 Hardware Platform

  **Waveshare ESP32-S3 Touch LCD 4.3" — Development Board (4.3)**

  > **Board variant note:** This project targets the **development board** (ESP32-S3-LCD-4.3), which has RS-485, CAN bus, and a USB host switch but **no audio subsystem, no RTC, and no optocoupler-isolated I/O**. A production/4.3C variant (AI Voice model with audio, RTC, optocouplers) exists but is not the target hardware. See the comparison table at the end of this section for differences. Schematics: `docs/ESP32-S3-Touch-LCD-4.3-Sch.pdf` (dev), `docs/production board/ESP32-S3-Touch-LCD-4.3C-Schematics.pdf` (production).

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
  - Backlight driver: **MP3302DJ-LF-Z** LED driver, controlled by **DISP** signal (EXIO2 via CH422G)

  **Touch**

  - **GT911** capacitive touch controller
  - I2C address 0x5D on shared bus (GPIO8=SDA, GPIO9=SCL, GPIO4=IRQ)
  - Reset via **EXIO1** (CTP_RST) on CH422G

  **I2C Expander**

  - **CH422G (U10)** — I2C I/O expander on same bus (GPIO8/9)
  - Uses fixed 8-bit command addresses (not standard 7-bit I2C addressing): 0x70 (OC output), 0x38 (push-pull output), 0x48 (set mode), 0x4D (read input)
  - Provides 8 bidirectional I/O pins (IO0-IO7 = EXIO0-EXIO7) and **4 general-purpose output pins (OC0-OC3)** — push-pull or open-drain selectable

  **CH422G EXIO Pin Assignments (development board):**

  | EXIO Pin | Net Name | Function |
  |----------|----------|----------|
  | EXIO0 | — | Unused / available |
  | EXIO1 | CTP_RST | Touch reset — pulse LOW→HIGH on boot |
  | EXIO2 | DISP | Display enable (backlight on/off via MP3302DJ) |
  | EXIO3 | LCD_RST | LCD panel reset |
  | EXIO4 | SDCS | SD card chip select |
  | EXIO5 | USB_SEL | USB host switch (FSUSB42UMX) — selects USB-JTAG vs USB host |

  **CH422G OC Output Pins (development board):**

  The CH422G has 4 additional output-only pins (OC0-OC3) that are separate from the 8 EXIO bidirectional pins. These can be push-pull or open-drain (selectable via set-mode command). On the dev board, OC0-OC3 are available for general-purpose output — used for feedback hardware (buzzer/LEDs).

  | OC Pin | CH422G Physical Pin | Available | Notes |
  |--------|-------------------|-----------|-------|
  | OC0 | Pin 8 | Yes | General-purpose output |
  | OC1 | Pin 9 | Yes | General-purpose output |
  | OC2 | Pin 10 | Yes | General-purpose output |
  | OC3 | Pin 11 | Yes | General-purpose output |

  **RS-485 Interface (development board only)**

  - **SP3485** RS-485 transceiver (U7) with 120Ω termination
  - **IO15** = RS485_TXD (Driver Input)
  - **IO16** = RS485_RXD (Receiver Output)
  - Terminal connector (J2): RS485_TX+, RS485_TX−, GND
  - DE/RE control via pull resistors (always enabled)
  - **Not used by this project.** IO15 and IO16 are available as GPIO when nothing is connected to the RS-485 terminal — the transceiver passes signals through harmlessly.

  **CAN Bus Interface (development board only)**

  - **TJA1051T/3/1J** CAN transceiver (U12)
  - Terminal connector (J4): CANH, CANL, 3V3
  - SM24CANB-02HTG TVS protection
  - **Not used by this project.**

  **USB Host Switch (development board only)**

  - **FSUSB42UMX** (U13) — USB 2.0 mux, controlled by **EXIO5** (USB_SEL)
  - Selects between USB-JTAG (native) and USB host mode
  - **Not used by this project** — EXIO5 left at default.

  **SD Card**

  - Micro SD card slot (SD1) — SPI interface, FAT32 formatted
  - **IO11** = MOSI, **IO12** = SCK, **IO13** = MISO
  - Chip select: **EXIO4** (via CH422G, bridged through R105 0R to IO10)
  - 128 GB card installed; used for database backups, usage history logs, and data export (see Section 5.7)

  **RFID**

  - **PN532 NFC Module V3** (13.56 MHz) — MIFARE Classic 1K tags
  - Connected via **I2C** on the shared bus (IO8=SDA, IO9=SCL), I2C address **0x24**
  - Powered from the board's I2C header (H8: VCC, GND, SDA, SCL)
  - Supports standard CFS v1 tags and extended v2 tags (see Section 7)

  **Sensor / ADC Input (development board only)**

  - **GPIO6** (ADC1_CH5) — routed to the **Sensor AD** header (J6, PH2.0 2-pin: GND, AD)
  - On-board voltage divider (÷3): input range 0–9.9V maps to 0–3.3V at the ADC pin
  - 12-bit resolution (0–4096), conversion: `voltage_mV = 3.3 / 4096 * 3 * adc_value * 1000`
  - **Not used by the display** — GPIO6 is I2S_MCLK on the 4.3C (audio), but on the dev board it's free and routed to this header
  - **Battery monitoring:** Wire VBAT (from battery connector J5 or CS8501 output) through the Sensor AD header to read battery voltage. The ÷3 divider maps a full 4.2V LiPo to ~1.4V at the ADC — well within range. No external components needed.

  **Available GPIO Pins**

  The following pins are available for general-purpose use on the development board:

  | GPIO | Board Function | Available For | Notes |
  |------|---------------|---------------|-------|
  | IO6 | Sensor AD header (J6) | **ADC input** (ADC1_CH5) | On-board ÷3 voltage divider. Not on display bus. Use for battery voltage monitoring. |
  | IO15 | RS485_TXD (via SP3485) | GPIO output | Safe when RS-485 terminal is disconnected. 3.3V output. |
  | IO16 | RS485_RXD (via SP3485) | GPIO input/output | Safe when RS-485 terminal is disconnected. 3.3V output. |

  > **Note:** IO43 and IO44 are UART0 TX/RX — not available. All other GPIOs are consumed by the display parallel bus, SD card SPI, or I2C.

  **Feedback Hardware**

  External indicators for RFID operation status feedback:

  - **Buzzer:** YMD-12095 active piezo buzzer (5V DC, continuous tone). Active buzzer has a built-in oscillator — apply voltage to sound, remove to silence. No PWM or tone generation needed, just digital HIGH/LOW.
  - **Red LED:** Standard 5mm red LED for failure/error indication
  - **Green LED:** Standard 5mm green LED for success indication

  **Wiring — Option A (recommended): CH422G OC outputs**

  The CH422G OC0-OC3 pins provide 4 dedicated output pins controllable via I2C. Use push-pull mode for direct drive. These output VCC level (3.3V or 5V depending on CH422G VCC rail).

  | Device | OC Pin | Wiring |
  |--------|--------|--------|
  | Buzzer (YMD-12095) | OC0 | OC0 → buzzer (+), GND → buzzer (−). Note: buzzer needs 5V; if CH422G VCC is 3.3V, use OC0 in open-drain mode with external 5V pull-up, or use a MOSFET level shifter. |
  | Red LED | OC1 | OC1 → 220Ω → red LED anode → GND |
  | Green LED | OC2 | OC2 → 220Ω → green LED anode → GND |

  **Wiring — Option B: Direct GPIO (IO15 / IO16) + one OC pin**

  Uses the RS-485 pins as GPIO (safe when RS-485 terminal disconnected) plus one CH422G OC pin for the 5V buzzer.

  | Device | Pin | Wiring |
  |--------|-----|--------|
  | Green LED | IO15 | IO15 → 220Ω → green LED → GND (3.3V) |
  | Red LED | IO16 | IO16 → 220Ω → red LED → GND (3.3V) |
  | Buzzer | OC0 | OC0 → buzzer (+), GND → buzzer (−). External 5V pull-up if OC open-drain mode. |

  > **Note:** A **single RGB NeoPixel (WS2812) LED** on IO15 or IO16 is also an option — one pin, any color (requires NeoPixel library, ~800 bytes RAM), eliminates the need for separate red/green LEDs.

  **Battery & Power**

  - **CS8501 (U2)** — LiPo charger + DC-DC boost converter (charges single-cell LiPo/18650 via USB and boosts to 5V for system power)
  - **J5** — PH2.0 2P battery connector for 3.7V LiPo / 18650 cell
  - Charge status LEDs: **CHG** (charging), **DONE** (complete), **PWR** (power on)
  - **SGM2212-3.3XKC3G/TR (U8)** — 3.3V LDO regulator from 5V
  - Board draws ~550mA for display alone; total system draw estimated ~700-800mA (display + ESP32 active + PN532)
  - Battery charging occurs via USB-C when connected; CS8501 handles charge/discharge management natively
  - **Battery voltage monitoring:** Wire VBAT to the **Sensor AD header** (J6, GPIO6/ADC1_CH5). The on-board ÷3 voltage divider maps 0–9.9V to ADC range. A full LiPo at 4.2V reads ~1.4V at the pin (~1745 ADC counts). Use a voltage-to-SoC lookup table for Li-ion discharge curve to estimate battery percentage.
  - **Note:** An external TP4056 charge/discharge step-up module (J5019) was evaluated but is **not required** — the on-board CS8501 provides equivalent functionality.

  **Buttons**

  - **K1** — RESET button (pulls RESET low via R38 10K)
  - **K2** — BOOT/IO0 button (pulls IO0 low for flash mode via R39 10K)

  **I2C Bus Summary (IO8=SDA, IO9=SCL)**

  All I2C peripherals share a single bus with 4.7K pull-ups (R100, R101) to I2C_VCC:

  | Device | I2C Address | Function |
  |--------|-------------|----------|
  | GT911 | 0x5D | Capacitive touch controller |
  | CH422G (U10) | Fixed command bytes (0x70, 0x48, 0x4D, etc.) | I/O expander (EXIO0-7 + OC0-3) |
  | PN532 (external) | 0x24 | NFC/RFID reader/writer |

  > **Note:** The dev board has only 3 I2C devices (vs 6 on production 4.3C). No RTC, no audio codecs. The CH422G address overlap note still applies: the 7-bit equivalent of command byte 0x48 is 0x24 (same as PN532). In practice this has not caused bus conflicts because the CH422G command protocol differs from standard I2C register access, but if issues arise, the PN532 V3 module supports SPI mode as an alternative. For I2C bus lockup detection, timeout handling, and SCL pulse recovery procedures, see Section 13.7.

  **Timestamps (no RTC)**

  The development board does **not** have an RTC. Timestamps for inventory weight history and spool creation dates require one of:
  - **NTP via WiFi** — accurate when connected; unavailable offline
  - **Relative uptime** — `millis()` since boot; resets on power cycle
  - **Manual date entry** — user sets date/time in Settings screen on first boot; stored in config, drifts without RTC

  For initial implementation, NTP is preferred when WiFi is available, with relative timestamps as fallback. Timestamp accuracy is non-critical — weight history entries are informational, not safety-critical.

  **USB**

  - Two USB-C ports: **USB-JTAG** (Type_C1) and **UART** (Type_C2, via CH343P USB-to-UART)
  - UART port used for upload/monitor (more stable)
  - `ARDUINO_USB_CDC_ON_BOOT=0` (CDC disabled, using hardware UART)
  - USB host switch (FSUSB42UMX) on EXIO5 — not used

  **Connectivity**

  - WiFi (via WiFiManager, currently disabled in code)
  - BLE (available but unused)

  **Board Definition**

  - Custom PlatformIO board: boards/waveshare_s3_43.json
  - Waveshare wiki: https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-4.3
  - Development board schematic: `docs/ESP32-S3-Touch-LCD-4.3-Sch.pdf`
  - Production board schematic: `docs/production board/ESP32-S3-Touch-LCD-4.3C-Schematics.pdf`
  - Chip datasheets: `docs/CH422DS1_EN.pdf`, `docs/GT911_EN_Datasheet.pdf`, `docs/ST7262.pdf`, `docs/TJA1051.pdf`, `docs/CH343DS1-en.pdf`
  - Board variant notes: `docs/board-variant-4.3C.md`

  **Development Board vs Production Board (4.3C) Comparison**

  | Feature | Dev Board (4.3) | Production Board (4.3C) |
  |---------|----------------|------------------------|
  | **Audio subsystem** | None | ES8311 + ES7210 + NS4150B + dual MEMS mics |
  | **RTC** | None | PCF85063ATL (I2C 0x51) |
  | **RS-485** | SP3485 on IO15/IO16 | None |
  | **CAN bus** | TJA1051T | None |
  | **USB host switch** | FSUSB42UMX on EXIO5 | None |
  | **Optocoupler I/O (P1)** | None | 2 inputs (DIN0/DIN1) + 2 outputs (DOUT0/DOUT1) |
  | **EXIO3 function** | LCD_RST | PA_CTRL (speaker amp enable) |
  | **EXIO5 function** | USB_SEL | DI1 (digital input) |
  | **EXIO6 function** | Available / unassigned | DOUT0 (optocoupler output) |
  | **EXIO7 function** | Available / unassigned | DOUT1 (optocoupler output) |
  | **EXIO_PWM** | Not connected | Backlight PWM (AP3032 boost driver) |
  | **EXIO_ADC** | Not connected (use GPIO6 Sensor AD instead) | VBAT sense (voltage divider R18/R19) |
  | **IO15** | RS485_TXD | I2S_DSDIN (audio DAC data) |
  | **IO16** | RS485_RXD | I2S_LRCK (audio L/R clock) |
  | **Backlight driver** | MP3302DJ-LF-Z (DISP on/off) | AP3032 (EXIO_PWM brightness) |
  | **3.3V regulator** | SGM2212-3.3 (U8) | TMI3112H (U8) |
  | **USB-to-UART** | CH343P | CH343G (assumed) |
  | **I2C devices** | 3 (GT911, CH422G, PN532) | 6 (+ PCF85063A, ES8311, ES7210) |
  | **Sensor/AD input** | Yes (analog header) | No |
  | **Physical buttons** | K1 (RESET), K2 (IO0/BOOT) | K1 (RESET), K2 (IO0/BOOT) |
  | **Battery charger** | CS8501 (U2) | CS8501 (U4) |

  > **Porting note:** The codebase and LGFX_Config.h work on both variants — the display bus, touch controller, CH422G init (EXIO1/EXIO2), and SD card are identical. Differences only affect feedback hardware wiring (no P1 header on dev), timestamps (no RTC on dev), and any code that references EXIO3/5/6/7 or audio codecs.

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

### 3.2 Data Model Separation

The system uses three distinct data structs that represent filament data at different abstraction levels. Keeping them separate prevents inventory logic from leaking tag format constraints, and isolates v1/v2 tag differences from the rest of the application.

```
┌─────────────────────┐     ┌──────────────────────┐     ┌──────────────────────┐
│  FilamentProfile    │     │     SpoolRecord       │     │      TagData         │
│  (library reference)│     │  (inventory record)   │     │  (RFID abstraction)  │
├─────────────────────┤     ├──────────────────────┤     ├──────────────────────┤
│ brand (32 chars)    │────▶│ profile (inline copy) │     │ version (v1/v2)      │
│ name (48 chars)     │     │ spool_id              │     │ tag_uid              │
│ material_type       │     │ tag_uid               │◀───▶│ sectors[] (raw)      │
│ color_hex/name      │     │ initial_weight_g      │     │ origin_magic         │
│ temps, speeds, fan  │     │ current_weight_g      │     │ payload_string (v1)  │
│ diameter, density   │     │ status                │     │ extended fields (v2) │
│ weight_g            │     │ source                │     │ crc32                │
│ is_custom           │     │ weight_history[]      │     │ mirrors[3]           │
└─────────────────────┘     └──────────────────────┘     └──────────────────────┘
        │                            │                            │
        │  "select from library"     │  "scan tag"                │
        └───────────────────────────▶│◀───────────────────────────┘
                                     │  "write tag"
                                     └───────────────────────────▶│
```

**FilamentProfile** (Section 5.3): Read-only reference from the Creality material database. Contains all known properties of a filament type. Loaded from JSON, cached in PSRAM. Never modified at runtime (except via DB update). Used as a template when creating new spools.

**SpoolRecord** (Section 6): Mutable inventory record for an owned spool. Contains an inline copy of filament properties (not a reference to FilamentProfile — survives DB updates). Tracks weight, usage history, status, and optional tag UID. Persisted to `/inventory.json`.

**TagData** (Section 7.9): Typed abstraction over raw RFID tag bytes. Encapsulates v1/v2 format differences, sector layout, mirror management, and CRC. The `rfid_driver` produces/consumes `TagData`; all other modules work with `SpoolRecord` or `FilamentProfile`.

**Conversion rules:**

| From | To | Conversion | Data Loss |
|------|----|-----------|-----------|
| FilamentProfile → SpoolRecord | Library select / custom entry | Copy fields into `profile` sub-object; set `source = "library"` or `"manual"` | None |
| TagData → SpoolRecord | RFID scan | Parse tag payload + extended sectors; set `source = "scan"`, `tag_uid` from tag | v1: brand/name lost (not on tag). v2: brand truncated to 12 chars, name to 16 chars. |
| SpoolRecord → TagData | RFID write | Serialize profile fields into tag payload string + extended sectors | Material type truncated to 5 chars. Brand to 12, name to 16 (v2 only). Temps/speeds/fan not written on v1. |
| TagData → FilamentProfile | Not used | — | — |
| SpoolRecord → FilamentProfile | Not used | — | — |
| FilamentProfile → TagData | Not directly | Must go through SpoolRecord first (SpoolRecord adds spool_id, weight tracking) | — |

This separation ensures:
- Tag format changes (v1→v2→future) are isolated to `TagData` and `rfid_driver`
- Inventory logic never needs to know about sector layouts or CRC
- UI screens work exclusively with `SpoolRecord` (inventory) or `FilamentProfile` (library)

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
- Parsed data copied into compact `std::vector<FilamentProfile>` cache (capped at 1000 profiles)
- JSON document discarded after load — only the cache persists at runtime
- Runtime cache resides in PSRAM; source files remain on LittleFS (gzipped) and SD card (raw)
- Peak memory during DB load: ~600 KB PSRAM (transient). See Section 14.1 for full memory budget.
- PSRAM allocation failures handled gracefully — see Section 14.3

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
3. "Test Connection" verifies printer is reachable via `GET /info`; store `printer_model` from response
4. On success, issue `GET /downloads/defData/material_database.json`:
   - Check `Content-Length` header before downloading body
   - If `Content-Length` > 512 KB → abort: "Database too large ({size} KB, max 512 KB)" — see Section 14.2
   - Allocate PSRAM buffer via `heap_caps_malloc`; if allocation fails → abort: "Not enough memory" — see Section 14.3
   - Stream response body into PSRAM buffer
5. **Compute SHA-256** of raw JSON buffer (via ESP32 hardware-accelerated `mbedtls_sha256`)
6. **Compare hash** to `config.db_hash`:
   - If identical → "Database is already up to date ({count} profiles)" — skip write, no changes
   - If different → proceed with update
7. **Validate schema** before committing:
   - Check for expected structure: `result.list[]` array with `base.{id, brand, name, meterialType}` and `kvParam.{nozzle_temperature}`
   - If structure matches known format → `db_schema_version = 1`
   - If top-level keys differ or `result.list` is missing → **schema change detected**: warn user "Database format has changed — {details}. Update anyway?" → [Proceed] / [Cancel]. If user proceeds, set `db_schema_version = 0` (unknown) and attempt best-effort parse.
8. Raw JSON saved to SD card: `/material_database.json` (backup, overwrites previous)
9. JSON gzip-compressed in memory, written to LittleFS: `/material_database.json.gz` (atomic: write temp file, rename)
10. FilamentDB cache reloaded from the new data
11. Update `config.json`: `db_hash`, `db_updated_at` (NTP timestamp or uptime), `db_profile_count`, `printer_model`, `db_schema_version`, `printer_ip`
12. UI displays: success status, profile count, timestamp, and whether schema version changed

**Error handling:**
- Network unreachable → "Cannot reach printer at {ip}" with retry option
- Invalid response (not JSON) → "Invalid database format" — previous DB preserved
- Schema validation failure (user cancelled) → previous DB preserved
- LittleFS write failure → "Storage error" — SD backup still available for manual recovery
- Download interrupted → previous database remains active (atomic replace via temp file)

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
| `/logs/usage_YYYYMMDD.csv` | Daily usage log files — see Section 5.9 for full spec |
| `/exports/` | User-initiated data exports |

**Benefits of this split:**
- LittleFS budget stays small (~100 KB total) — no flash wear concerns
- SD card handles unbounded data (full usage history, multiple backups) without memory caps
- Weight history in `inventory.json` can be kept small (last 10 entries per spool) since the full log lives on SD
- Raw JSON backup on SD enables manual recovery if LittleFS gets corrupted

### 5.9 SD Card Usage Log

#### Format

CSV with comma delimiter, UTF-8 encoding, `\n` (LF) line endings. Each file covers one calendar day (UTC from RTC).

**File path pattern:** `/logs/usage_YYYYMMDD.csv` (e.g., `/logs/usage_20260214.csv`)

**Header row** (written when file is created):
```
#v1,spool_id,event,weight_g,timestamp,notes
```

The `#v1` prefix serves as a **format version tag**. Future format changes increment this (`#v2`, etc.), allowing analytics tools to parse files correctly across firmware versions.

**Data rows:**

| Column | Type | Description |
|--------|------|-------------|
| `spool_id` | string | e.g., `SPL-0042` |
| `event` | enum | `WEIGHT_UPDATE`, `CREATED`, `ARCHIVED`, `DELETED`, `REACTIVATED`, `TAG_SYNCED` |
| `weight_g` | uint32 | Current weight at time of event (0 for non-weight events) |
| `timestamp` | ISO 8601 | `YYYY-MM-DDTHH:MM:SSZ` (UTC from RTC). ISO 8601 chosen over Unix epoch for human readability when SD card is read on a PC. |
| `notes` | string | Optional context, quoted if contains commas. e.g., `"from_tag"`, `"manual_entry"`, `""` |

**Example:**
```csv
#v1,spool_id,event,weight_g,timestamp,notes
SPL-0001,WEIGHT_UPDATE,750,2026-02-14T10:30:00Z,"manual_entry"
SPL-0001,WEIGHT_UPDATE,620,2026-02-14T14:15:00Z,"from_tag"
SPL-0002,CREATED,1000,2026-02-14T14:20:00Z,"source:scan"
SPL-0003,ARCHIVED,0,2026-02-14T16:00:00Z,""
SPL-0004,DELETED,0,2026-02-14T16:05:00Z,""
```

#### Log Rotation

- **One file per day** — new file created when the date changes (checked on first write after midnight UTC)
- File size is naturally bounded: at ~80 bytes per row, 1000 events/day = ~80 KB/day
- No automatic deletion of old log files — 128 GB SD card holds years of logs
- Future: export UI could offer "Delete logs older than N days" option

#### Write Atomicity & SD Removal Safety

SD card FAT32 appends are **not atomic** — a power loss or card removal mid-write can corrupt the last line or the FAT.

**Mitigations:**

| Risk | Mitigation |
|------|-----------|
| Power loss mid-write | Each log entry is a single short line (~80 bytes). `file.flush()` called after every append. Corruption limited to at most the last line — all prior entries intact. |
| SD card removed mid-write | SD operations wrapped in a try/check pattern: verify `SD.exists()` before open. If write fails, set `sd_available = false` flag, skip future SD writes until next `sd_check()` (periodic, every 60s). Log entry lost but LittleFS inventory is unaffected. |
| FAT corruption from dirty unmount | On boot, if SD mount fails after previously working → warn user: "SD card may need formatting. Inventory is safe on internal storage." |
| File grows too large (single day with thousands of events) | Unlikely in normal use. Safety cap: if single file exceeds **1 MB**, close and start a new file with suffix: `usage_YYYYMMDD_2.csv` |

**Non-fatal principle:** All SD log write failures are non-fatal warnings. The system never blocks or enters ERROR state due to SD card issues. LittleFS inventory is the authoritative data store — SD logs are supplementary.

#### SD Card Availability Check

```cpp
// Periodic check (called from loop() every 60 seconds)
void sd_check() {
    bool was_available = sd_available;
    sd_available = SD.exists("/");  // Quick mount check
    if (!was_available && sd_available) {
        Serial.println("SD card reinserted");
        // Resume logging — no recovery needed, just start appending
    }
    if (was_available && !sd_available) {
        Serial.println("SD card removed");
        // Non-fatal — disable SD features until redetected
    }
}
```

### 5.8 FilamentProfile to SpoolData Mapping

> **Note:** The current codebase uses a single `SpoolData` class (in `include/spool_data.h`) that combines inventory state and tag serialization. Per the data model separation (Section 3.2), this will be refactored into `SpoolRecord` (inventory) + `TagData` (RFID abstraction). The mapping table below uses the current `SpoolData` field names for reference, with the target model in parentheses.

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
save()                              — Persist full inventory to LittleFS (atomic write — see Section 6.9)
addSpool(profile, weight, uid)      — Create new inventory record; returns spool_id
archiveSpool(spool_id)              — Set status to "archived" (soft delete, reversible — see Section 6.10)
deleteSpool(spool_id)               — Permanently remove spool record (irreversible — see Section 6.10)
updateWeight(spool_id, weight)      — Update current_weight_g; append to weight_history
getSpoolByUID(tag_uid)              — Lookup spool by RFID tag UID; returns nullptr if not found
getSpoolById(spool_id)              — Lookup spool by internal ID
getAllActive()                       — Return vector of active (non-archived) spools
getSpoolCount()                     — Total number of active spools
```

### 6.7 Tag ↔ Inventory Reconciliation

When a scanned RFID tag matches an existing inventory spool (by `tag_uid`), the tag's weight (sectors 6-8) and the inventory's `current_weight_g` may disagree — for example, after the Creality K2 Plus printer updates the tag weight during a print while the inventory remains stale.

**Policy: Prompt on mismatch (Option C)**

```
1. User scans tag
2. System reads tag weight from sectors 6-8
3. System looks up spool by tag_uid in inventory
4. If found AND tag_weight ≠ inventory_weight:
   → Show dialog:
     "Tag: 620g | Inventory: 750g"
     [Use Tag Weight]  [Keep Inventory]  [Cancel]
5. If user selects "Use Tag Weight":
   → updateWeight(spool_id, tag_weight) — updates inventory, appends history
6. If user selects "Keep Inventory":
   → No change to inventory; optionally offer to write inventory weight back to tag
7. If weights match:
   → No prompt, proceed directly to Spool Detail
```

**Threshold:** A tolerance of ±5g avoids prompts from rounding differences (tag stores weight as length in mm with integer precision). Only prompt when `abs(tag_weight - inventory_weight) > 5`.

**New tag (not in inventory):**
- Tag scanned but `tag_uid` not found in inventory → auto-create new spool record from tag data with `source: "scan"`, weight from tag sectors 6-8.

**Untagged inventory spool:**
- Spool has no `tag_uid` (local-only, created via manual entry) → reconciliation does not apply.

### 6.8 UID Uniqueness & Collision Handling

**Invariant:** A `tag_uid` must be unique among all **active** (non-archived) spools in the inventory. Archived spools may retain their `tag_uid` for historical reference but are excluded from UID lookups.

**Lookup order for `getSpoolByUID(tag_uid)`:**
1. Search active spools only (status = `"active"` or `"empty"`)
2. If no active match, search archived spools
3. If no match at all, treat as new tag

**Collision scenarios and policies:**

| Scenario | Detection | Policy |
|----------|-----------|--------|
| **Duplicate UID among active spools** | `addSpool()` finds existing active spool with same UID | Error: "This tag is already assigned to spool {spool_id}. Remove existing assignment first." Reject the add. |
| **Tag reinitialized / rewritten** | Scanned tag data (material, color) differs significantly from inventory record for that UID | Prompt: "Tag data doesn't match inventory record '{name}'. Replace inventory entry with new tag data?" → [Replace] [Keep Old] [Add as New Spool] |
| **Archived spool rescanned** | UID matches an archived spool, no active match | Prompt: "This tag was previously archived as '{name}'. Reactivate it?" → [Reactivate] (sets status back to `"active"`, updates weight from tag) / [Create New] (new spool record, old stays archived) |
| **Cloned tag (duplicate UID)** | User writes same UID to two physical tags (rare, requires specialized hardware) | System cannot distinguish clones from the original. Only one inventory record per UID. User must manage manually — remove UID from one spool via Spool Detail ("Unlink Tag" action). |

**Tag reassignment flow (Write to Tag from Spool Detail):**
When a user writes spool data to a new/different tag:
1. The new tag's UID is read during the write operation
2. If that UID is already assigned to a different active spool → error, abort write
3. If write succeeds, the spool's `tag_uid` is updated to the new UID
4. The old tag (if any) is now orphaned — scanning it will trigger the "new tag" flow

**"Unlink Tag" action (Spool Detail screen):**
Clears the spool's `tag_uid` to empty string, making it a local-only spool. The physical tag remains unchanged but will be treated as a new/unrecognized tag on next scan.

### 6.9 Save Atomicity

Inventory is the most critical persistent data on the device — losing it means losing all spool records and weight history. A raw overwrite (`open → write → close`) risks corruption if power is lost mid-write. All inventory writes use atomic replace semantics.

**Atomic write procedure (`save()`):**

```
1. Serialize cache to JSON in PSRAM buffer
2. Write to temp file: /inventory.json.tmp
3. Flush to flash: file.flush() (fsync equivalent on LittleFS)
4. Close temp file
5. Remove old file: LittleFS.remove("/inventory.json")
6. Rename: LittleFS.rename("/inventory.json.tmp", "/inventory.json")
```

**Failure modes:**

| Failure Point | State After Power Restore | Recovery |
|---------------|--------------------------|----------|
| During step 2-4 (writing temp) | `/inventory.json` intact, `/inventory.json.tmp` partial | `init()` deletes orphaned `.tmp` file, loads existing inventory |
| During step 5 (remove old) | Both files may exist, or old is gone + temp present | `init()` checks: if `.tmp` exists and `/inventory.json` is missing, rename `.tmp` → `.json` |
| During step 6 (rename) | Old removed, `.tmp` exists but not yet renamed | Same as above — `init()` recovers from `.tmp` |

**`init()` recovery logic:**

```
1. If /inventory.json.tmp exists:
   a. If /inventory.json also exists → delete .tmp (interrupted before remove)
   b. If /inventory.json missing → rename .tmp → .json (interrupted after remove)
2. Load /inventory.json normally
3. If load fails (corrupt) → attempt to load latest backup from SD card
4. If no backup → create empty inventory
```

**SD card backup:**
After every successful atomic save to LittleFS, a timestamped backup is written to SD card: `/backups/inventory_YYYYMMDD_HHMMSS.json`. SD write failure is non-fatal — LittleFS is the primary store. Old backups are retained (not overwritten) so the user can recover from any historical state.

**This same atomic write pattern applies to:**
- `/material_database.json.gz` (Section 5.6, step 9)
- `/config.json` (lower risk due to small size, but same pattern for consistency)

### 6.10 Archive vs Delete Behavior

Two distinct removal operations with different consequences:

**Archive (reversible, soft delete):**

| Aspect | Behavior |
|--------|----------|
| **Action** | `archiveSpool(spool_id)` sets `status` to `"archived"`, sets `updated_at` to current timestamp (NTP or uptime) |
| **Inventory JSON** | Spool record **remains** in `/inventory.json` with `status: "archived"` |
| **UID association** | `tag_uid` **retained** on the archived record. Excluded from active UID lookups but available for reactivation (see Section 6.8 "Archived spool rescanned"). |
| **SD usage history** | Unchanged — all prior entries in `/logs/usage_YYYYMMDD.csv` preserved |
| **RAM cache** | Record remains in `std::vector<SpoolRecord>` but filtered out of `getAllActive()` |
| **Reversibility** | Fully reversible — scanning the linked tag or a future "Unarchive" action sets status back to `"active"` |
| **UI visibility** | Hidden from default inventory list. Visible via "Show archived" filter (future) or when the linked tag is rescanned. |
| **Confirmation** | Single tap — no confirmation dialog (easy to undo) |

**Delete (irreversible, hard delete):**

| Aspect | Behavior |
|--------|----------|
| **Action** | `deleteSpool(spool_id)` permanently removes the record from the inventory cache |
| **Inventory JSON** | Spool record **removed** from `/inventory.json` on next `save()` |
| **UID association** | Cleared — the physical tag becomes unrecognized. Scanning it will trigger the "new tag" flow (Section 6.7). |
| **SD usage history** | **Preserved** — entries in `/logs/usage_YYYYMMDD.csv` are never deleted (append-only log). The `spool_id` in the log remains as a historical reference even though the inventory record is gone. |
| **SD deletion record** | A deletion event is appended to the usage log: `{spool_id}, DELETED, {timestamp}` |
| **RAM cache** | Record removed from `std::vector<SpoolRecord>` |
| **Reversibility** | **Irreversible** from the device. Recovery only possible by restoring an SD card backup (Section 6.9). |
| **UI visibility** | Gone from all views |
| **Confirmation** | **Two-step confirmation required:** first tap shows dialog "Permanently delete '{name}'? This cannot be undone." → [Delete] / [Cancel]. Destructive button styled red. |

**Decision guide (shown in Spool Detail):**

The UI should make the distinction clear:
- **Archive** button: normal styling, labeled "Archive" — for spools you're done with but might reuse
- **Delete** button: red/destructive styling, labeled "Delete" — for spools added by mistake or duplicates

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
| 0x0C | 4 | **Origin magic** | `0x4B324658` ("K2FX" ASCII, little-endian) |

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

**Origin detection (v2 tag ownership):**

Sector 10, offset 0x0C contains a 4-byte **origin magic**: `0x4B324658` (ASCII `"K2FX"`, stored little-endian). This marker identifies tags whose extended sectors (10-13) were written by this system.

| Version Byte | Origin Magic | Interpretation | Behavior |
|-------------|-------------|----------------|----------|
| `0x01` | N/A (sectors 10-13 not read) | Standard CFS v1 tag (Creality original) | Read sectors 1-9 only. Safe to write v2 extended data if user explicitly requests "Save + Write to tag" (upgrades tag to v2). |
| `0x02` | `0x4B324658` ("K2FX") | Extended v2 tag written by this system | Read sectors 1-13. Safe to overwrite extended sectors on write. |
| `0x02` | Any other value | Extended v2 tag from unknown/third-party system | Read sectors 1-9 only (ignore 10-13). **Do not overwrite** sectors 10-13. Warn user: "Tag has extended data from another system. Only standard fields will be used." Write operations write sectors 1-9 only (v1 behavior). |
| `0x02` | `0x00000000` (all zeros) | Possibly uninitialized extended sectors | Treat as foreign — same as "any other value" above. |

This prevents silently corrupting tags that were programmed by third-party tools that also use sectors 10-13 with a different layout.

### 7.6 CRC32 Checksum (Sector 15)

**Location:** Sector 15, Block 60, offset 0x00 — 4 bytes, little-endian.

**Algorithm:**
- **Standard:** CRC-32/ISO-HDLC (IEEE 802.3), same as zlib `crc32()`
- **Polynomial:** 0x04C11DB7 (normal form) / 0xEDB88320 (reflected, as used in lookup table implementations)
- **Initial value:** 0xFFFFFFFF
- **Final XOR:** 0xFFFFFFFF
- **Input/output reflection:** Yes (standard reflected CRC-32)
- **Stored endianness:** Little-endian (LSB at offset 0x00)

**Coverage:**

| Tag Version | CRC Input Data |
|-------------|----------------|
| v1 (0x01) | Data blocks of sectors 1–9 (36 blocks × 16 bytes, excluding sector trailers) |
| v2 (0x02) | Data blocks of sectors 1–13 (52 blocks × 16 bytes, excluding sector trailers) |

**Excluded from CRC:**
- Sector 0 (manufacturer / UID block — read-only, cannot be part of integrity check)
- Sector trailers (block 3 of each sector — contain Key A/B and access bits, not data)
- Sector 14 (reserved, must not be modified)
- Sector 15 itself (contains the CRC)

**Per-sector, 3 of 4 blocks are data blocks** (blocks 0, 1, 2). Block 3 is the sector trailer. So:
- v1: sectors 1–9 = 9 sectors × 3 data blocks × 16 bytes = **432 bytes** input to CRC
- v2: sectors 1–13 = 13 sectors × 3 data blocks × 16 bytes = **624 bytes** input to CRC

**Validation:** On read, compute CRC over the appropriate sectors (based on version byte) and compare to stored value. Mismatch → tag flagged as potentially corrupt, user warned.

**Update:** CRC must be recomputed and written to sector 15 after any write to sectors 1–13. This is the final step of the write transaction.

### 7.7 Write Semantics
All writes are transactional: authenticate → read existing → validate → write → read back → byte-compare → update CRC → confirm. Partial writes are forbidden. All three mirrors (sectors 6-8) must be updated consistently. For v2 tags, sectors 10-13 are also written atomically with the CRC update.

Full specification: `docs/rfid/creality-k2plus-rfid-spec.md`

### 7.8 Read Integrity: Mirror Voting & CRC Validation

Sectors 6-8 contain three mirrored copies of the remaining filament weight/length. If power is lost during a write (by the printer or this device), some mirrors may be updated while others are stale or corrupted. The system uses **majority voting** to recover a reliable value.

**Mirror read procedure:**

```
1. Read all 3 mirrors: sector 6 → M0, sector 7 → M1, sector 8 → M2
2. Compare data blocks byte-for-byte:
   a. If M0 == M1 == M2 → unanimous, use any (prefer M0)
   b. If two match, one differs → use majority value
      - M0 == M1, M2 differs → use M0 (M2 likely stale/corrupt)
      - M0 == M2, M1 differs → use M0
      - M1 == M2, M0 differs → use M1
   c. If all three differ → tag is corrupt (see below)
3. Log which mirrors disagreed (Serial debug output)
```

**CRC validation (combined with mirror check):**

| Mirror Status | CRC Status | Interpretation | Action |
|--------------|-----------|----------------|--------|
| All 3 match | CRC valid | Tag is healthy | Use data normally |
| All 3 match | CRC invalid | CRC stale (power-fail after mirrors written, before CRC) | Use mirror data (it's consistent). Offer to repair: "Tag CRC is outdated. Repair?" → rewrite CRC. |
| 2 of 3 match | CRC valid | One mirror corrupt (partial write) | Use majority value. Offer to repair: "One mirror is inconsistent. Repair?" → rewrite the bad mirror. |
| 2 of 3 match | CRC invalid | Partial write interrupted CRC update | Use majority value. Offer to repair CRC + bad mirror. |
| All 3 differ | CRC valid | Severe corruption but CRC matches something | Compute CRC against each mirror individually. Use the one that matches CRC. If none match → corrupt. |
| All 3 differ | CRC invalid | **Tag is corrupt** | Warn user: "Tag data is corrupted (all mirrors disagree, CRC invalid). The tag cannot be trusted." Do **not** use data. Do **not** add to inventory. Offer: [Write New Data] (reinitialize from inventory/library) or [Dismiss]. |

**Power-fail during write — failure point analysis:**

| Failure Point | Tag State After Power Restore | Recovery |
|--------------|-------------------------------|----------|
| After sector 6 written, before 7-8 | M0 = new, M1 = old, M2 = old | Majority voting → old value (safe). Next full write repairs all 3. |
| After sectors 6-7 written, before 8 | M0 = new, M1 = new, M2 = old | Majority voting → new value (correct). Sector 8 stale but outvoted. |
| After sectors 6-8 written, before CRC | M0 = M1 = M2 = new, CRC = old | Mirrors are consistent. CRC mismatch detected → offer repair. |
| After CRC written | Fully consistent | No issue. |
| Mid-block write (within a single sector) | Block may contain partial data | MIFARE Classic writes are block-atomic (16 bytes). Mid-block corruption does not occur at the protocol level — the tag controller commits or discards the full 16-byte block. |

**Write order (maximizes recoverability):**
```
Sector 6 → Sector 7 → Sector 8 → Sector 9 (usage counters) → Sector 15 (CRC)
```
Mirrors are written in order 6→7→8 so that at any interruption point, majority voting yields either the old (safe) or new (correct) value — never garbage.

### 7.9 TagData Structure

`TagData` is the typed abstraction over raw RFID tag bytes. It encapsulates all v1/v2 format differences so that no other module needs to understand sector layouts, byte offsets, or CRC mechanics. See Section 3.2 for how it relates to `FilamentProfile` and `SpoolRecord`.

```cpp
struct TagData {
    // Identity
    uint8_t  uid[7];                // 7-byte MIFARE UID
    uint8_t  uid_length;            // 4 or 7
    uint8_t  version;               // 0x01 = v1, 0x02 = v2

    // v1 payload (parsed from sectors 1-4)
    char     payload_string[41];    // Fixed-length CFS payload (null-terminated)
    char     material_type[6];      // 5 chars + null (from payload pos 12-16)
    uint32_t color_hex;             // Parsed from payload pos 18-23
    uint16_t material_length_mm;    // Parsed from payload pos 24-27

    // Mutable data (sectors 6-8 mirrors, sector 9)
    uint16_t remaining_length_mm;   // From mirror voting (Section 7.8)
    uint32_t usage_counter;         // Sector 9 usage count

    // v2 extended fields (sectors 10-13, only populated if version == 0x02)
    bool     has_extended;          // true if v2 with K2FX origin magic
    uint16_t nozzle_temp_min;
    uint16_t nozzle_temp_max;
    uint16_t bed_temp_min;
    uint16_t bed_temp_max;
    uint16_t nozzle_temp_default;
    uint16_t bed_temp_default;
    uint16_t print_speed_min;
    uint16_t print_speed_max;
    uint8_t  fan_percent;
    uint16_t max_volumetric_flow;   // ×10 (e.g., 240 = 24.0 mm³/s)
    uint16_t diameter_um;
    uint16_t density_x100;          // ×100 (e.g., 127 = 1.27 g/cm³)
    char     brand[13];             // 12 chars + null (sector 12)
    char     product_name[17];      // 16 chars + null (sector 13)
    uint32_t origin_magic;          // Sector 10 offset 0x0C (0x4B324658 = "K2FX")

    // Integrity
    uint32_t crc32_stored;          // CRC from sector 15
    uint32_t crc32_computed;        // CRC computed over read data
    bool     crc_valid;             // crc32_stored == crc32_computed
    uint8_t  mirror_agreement;      // 3 = unanimous, 2 = majority, 0 = all differ
};
```

**rfid_driver API using TagData:**

| Function | Direction | Description |
|----------|-----------|-------------|
| `readTag(TagData& out)` | Tag → TagData | Reads all sectors, performs mirror voting (7.8), validates CRC (7.6), populates all fields. Returns false on auth/read failure. |
| `writeTag(const TagData& in)` | TagData → Tag | Writes payload to sectors 1-9, extended to 10-13 (if v2), CRC to 15. Follows write order (7.8). Returns false on failure. |
| `tagDataFromSpool(const SpoolRecord& spool, TagData& out)` | SpoolRecord → TagData | Serializes spool fields into TagData: generates payload string, populates extended fields, sets version/origin. |
| `spoolFromTagData(const TagData& tag, SpoolRecord& out)` | TagData → SpoolRecord | Parses TagData fields into SpoolRecord: converts length→weight, maps extended fields to profile, sets `source = "scan"`. |

This keeps all byte-level tag logic in `rfid_driver` and `TagData`. The UI, inventory manager, and state machine never touch raw sector data.

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
| `printer_model` | string | Printer model from last `/info` response (e.g., `"F008"` = K2 Plus, `"F018"` = Hi) |
| `db_updated_at` | uint32 | Unix timestamp of last successful database update |
| `db_profile_count` | uint16 | Number of profiles in current database |
| `db_hash` | string | SHA-256 hex digest of the raw JSON as downloaded (64 chars). Used to detect if a re-download is identical to the current DB. |
| `db_schema_version` | uint8 | Schema version detected during last parse (see Section 5.6). `1` = known Creality format (`result.list[].base` + `result.list[].kvParam`). Incremented if structural changes are detected. |

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

Wiring depends on chosen option (see Section 2.1 Feedback Hardware). Recommended: buzzer on CH422G OC0, LEDs on OC1/OC2 or IO15/IO16.

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
- **CH422G access:** If using OC0-OC3 outputs, the CH422G expander must remain initialized across the application lifecycle (currently it's a local variable in `init_ch422g_4_3c()`). The expander instance should be promoted to a global or managed by a shared driver. OC output is controlled via I2C command byte 0x70.
- **Active buzzer at 5V:** The YMD-12095 is rated for 5V DC. If connected via CH422G OC pin in open-drain mode with external 5V pull-up, the buzzer gets full voltage. If connected directly to an ESP32 GPIO (3.3V), the buzzer may sound at reduced volume or not trigger — test before committing.
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
- Scan Tag → initiate RFID read → if tag found in inventory, check weight reconciliation (Section 6.7) then show detail; if new, auto-add to inventory with `source: "scan"`
- Add Custom → navigate to Custom Entry screen
- List supports scrolling for large inventories

### 11.4 Spool Detail Screen

**Purpose:** View and edit an individual spool's data.

**Layout:**
- Header: Brand + filament name, color swatch, material type
- Info section: temperature ranges, speed range, fan %, diameter, density
- Weight gauge: circular/bar gauge showing `current_weight_g` vs `initial_weight_g` with percentage
- Weight history: last N entries displayed as a simple list or mini chart
- Action buttons: Update Weight, Write to Tag, Unlink Tag, Archive/Delete

**Behavior:**
- Update Weight → opens number input dialog; value saved to inventory with timestamp
- Write to Tag → navigates to main/write screen pre-populated with this spool's data; if target tag UID differs from spool's current UID, reassigns (see Section 6.8)
- Unlink Tag → clears `tag_uid` from spool, making it local-only (see Section 6.8). Only shown if spool has a linked tag.
- Archive → see Section 6.10 for full archive behavior
- Delete → see Section 6.10 for full delete behavior

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
- Brightness slider (dev board: on/off only via EXIO2/DISP; 4.3C: PWM dimming via EXIO_PWM)
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

    // Error recovery
    USER_DISMISS,          // User taps OK/Dismiss on error dialog
    USER_RETRY,            // User taps Retry on error dialog

    // System
    BATTERY_CRITICAL,      // Battery level critical (future)
    TIMEOUT,               // Operation or error auto-dismiss timeout
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
| ERROR | USER_DISMISS | IDLE |
| ERROR | USER_RETRY | *(returns to the operation that failed — see Section 12.4)* |
| ERROR | TIMEOUT | IDLE |

### 12.4 Error Recovery Semantics

The ERROR state is entered when any operation fails (RFID read/write, tag verification, DB update). It is **not blocking** — the LVGL event loop continues running so the UI remains responsive and the error dialog is interactive.

**Error dialog UI:**

```
┌─────────────────────────────────┐
│  ⚠ Operation Failed             │
│                                 │
│  {error message}                │
│  e.g., "Write failed: no tag    │
│  detected on reader"            │
│                                 │
│  [Retry]  [Dismiss]       5s ⏱ │
└─────────────────────────────────┘
```

- Modal overlay on the top layer (blocks interaction with underlying screen)
- Error message describes what failed and why (see error message table below)
- Two buttons: **Retry** and **Dismiss**
- Countdown timer (top-right or bottom-right): auto-dismisses after timeout

**Timeouts:**

| Error Source | Auto-Dismiss Timeout | Rationale |
|-------------|---------------------|-----------|
| RFID read failure | 5 seconds | Quick retry expected — user just needs to place tag |
| RFID write failure | 10 seconds | User may need to reposition tag or check it |
| RFID verify failure | 10 seconds | Potentially serious — give user time to read message |
| DB update failure (network) | 15 seconds | User may need to check WiFi/IP — longer read time |
| DB update failure (schema) | No auto-dismiss | Requires user decision (proceed or cancel) |
| Inventory save failure | No auto-dismiss | Critical error — user must acknowledge data may not be saved |

**Auto-dismiss behavior:**
- Countdown displayed as "5s", "4s", "3s"... next to the Dismiss button
- When timer reaches 0, fires `TIMEOUT` event → transitions to IDLE
- Any user interaction (tap Retry or Dismiss) cancels the countdown
- If `beep_enabled`, a short beep sounds when auto-dismiss fires (so user notices even if not looking)

**Retry behavior:**

The ERROR state records which operation failed (`error_source`). When the user taps Retry:

| Error Source | Retry Action |
|-------------|-------------|
| READING_TAG | Re-enter READING_TAG → call `rfid.readCFSTag()` again |
| WRITING_TAG | Re-enter WRITING_TAG → call `rfid.writeCFSTag()` again with same `currentSpool` |
| VERIFYING_TAG | Re-enter VERIFYING_TAG → read back and compare again |
| UPDATING_DATABASE | Re-enter UPDATING_DATABASE → retry download from same printer IP |
| Inventory save | Retry `save()` (atomic write) |

Retry returns to the failed operation's state, **not** to IDLE. If the retry also fails, ERROR is re-entered with a fresh timeout. There is no retry limit — the user can retry indefinitely or dismiss at any time.

**Dismiss behavior:**
- Fires `USER_DISMISS` event → transitions to IDLE
- Returns to the screen the user was on before the operation started
- No data changes — the failed operation has no side effects (writes are transactional, DB updates use atomic replace)

**Error context stored in state machine:**

```cpp
struct ErrorContext {
    SystemState failed_state;       // Which state failed (READING_TAG, WRITING_TAG, etc.)
    String message;                 // Human-readable error description
    uint16_t auto_dismiss_ms;       // 0 = no auto-dismiss
    bool retryable;                 // false hides the Retry button
};
```

**Non-retryable errors** (Retry button hidden):
- LittleFS mount failure at boot (fatal — requires restart)
- SD card format error (non-fatal but not retryable in place)

### 12.5 Operation Lock & UI Guard

The state machine implicitly acts as a **global operation lock** — all destructive operations (RFID read/write, DB update, inventory save) can only begin from IDLE. The UI must enforce this by disabling interaction during non-IDLE states.

**Rule:** When `sysState.current() != IDLE`, all action buttons that trigger state transitions are **disabled** (greyed out, non-clickable). Only navigation that doesn't trigger operations (e.g., back buttons to cancel) remains active.

**Per-screen button guard behavior:**

| Screen | Buttons Disabled When Not IDLE | Always Active |
|--------|-------------------------------|---------------|
| Main/Write | READ, WRITE, Library, color picker, weight slider | Settings (navigate away is safe) |
| Inventory | Scan Tag, Add Custom | Settings, scroll, tap row (view only) |
| Spool Detail | Update Weight, Write to Tag, Unlink Tag, Archive, Delete | Back |
| Custom Entry | Save, Save + Write | Back, Cancel, step navigation |
| Update Database | Test Connection, Download | Back |
| Settings | WiFi Setup, Update Database, Restart | Back, Beep toggle, About |

**Implementation:**

```cpp
// In event_handler, gate all operation-triggering actions:
if (sysState.current() != SystemState::IDLE) {
    return;  // Reject input — system is busy
}
// Proceed with state transition...
sysState.transition(SystemEvent::WRITE_REQUEST);
```

**Visual feedback during busy states:**
- Disabled buttons use `LV_STATE_DISABLED` style (greyed out, reduced opacity)
- Status label shows current operation: "Reading tag...", "Writing tag...", "Downloading database..."
- Optional: spinner/progress indicator on active operation

**Specific scenarios prevented:**

| Scenario | Prevention |
|----------|-----------|
| User taps WRITE twice rapidly | First tap transitions to WRITING_TAG; second tap rejected (not IDLE) |
| User navigates to Library mid-write | Library button disabled during WRITING_TAG |
| User taps READ during DB download | READ button disabled during UPDATING_DATABASE |
| User taps Scan during a write from Spool Detail | Scan button disabled (not IDLE) |
| WiFi portal launched during RFID operation | WiFi Setup button disabled (not IDLE) |

**Re-enable:** Buttons are re-enabled when the state machine returns to IDLE (via OPERATION_SUCCESS, OPERATION_FAILED → ERROR → TIMEOUT → IDLE, or DB_UPDATE_SUCCESS/FAILED).

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

### 13.7 I2C Bus Recovery

The shared I2C bus (IO8=SDA, IO9=SCL) connects multiple devices (dev board: GT911 touch, CH422G expander, PN532 RFID; production 4.3C adds PCF85063A RTC, ES8311 codec, ES7210 ADC). Bus lockups can occur when a device holds SDA low after an interrupted transaction — a common embedded I2C failure mode.

**Timeout Detection**

All I2C transactions use a bounded timeout. The ESP32 I2C peripheral and Wire library support configurable timeouts:

| Transaction Type | Timeout | Action on Timeout |
|-----------------|---------|-------------------|
| PN532 command/response | 1000 ms | Increment failure counter; trigger bus recovery if counter ≥ 2 |
| CH422G EXIO write | 200 ms | Log warning; retry once; trigger bus recovery on second failure |
| GT911 touch read | 100 ms | Skip frame (touch data is non-critical); trigger recovery after 5 consecutive failures |

**SCL Clock Pulse Recovery (Standard I2C Recovery)**

When a device holds SDA low (bus stuck), the standard recovery procedure is to clock SCL manually until the device releases SDA:

1. Release I2C peripheral (call `Wire.end()`)
2. Configure IO9 (SCL) as GPIO output, IO8 (SDA) as GPIO input
3. Clock SCL high/low for **9 pulses** (max one byte + ACK) with 5 µs half-period
4. After each rising edge, check if SDA is high (released)
5. If SDA released → generate STOP condition (SDA low→high while SCL high)
6. If SDA still low after 9 pulses → recovery failed, proceed to full bus reset
7. Re-initialize Wire (`Wire.begin(SDA, SCL, 400000)`)
8. Log recovery attempt and outcome

**PN532 Reset Procedure**

If SCL pulse recovery does not resolve PN532 communication failures:

1. Power-cycle the PN532 module (if hardware reset pin is wired; otherwise skip)
2. Re-initialize Wire bus (`Wire.end()` → `Wire.begin()`)
3. Re-run PN532 initialization sequence (`pn532.begin()`, `pn532.SAMConfig()`)
4. Verify communication with `pn532.getFirmwareVersion()`
5. If verification fails → set RFID status to "unavailable", notify user "RFID module not responding — check connection"

> **Note:** The PN532 V3 module does not expose a hardware reset pin in the standard I2C header. A full reset requires either power-cycling the module (via a GPIO-controlled MOSFET on VCC, not currently wired) or physical reconnection. The software recovery (bus re-init + SAMConfig) resolves most bus lockup scenarios.

**CH422G Re-initialization**

The CH422G controls critical functions (backlight, touch reset, digital outputs for buzzer/LEDs). If CH422G communication fails:

1. Perform SCL pulse recovery (above)
2. Re-send CH422G mode configuration (set-mode command 0x48)
3. Re-write last known EXIO output state (DISP, OC0-OC3 for feedback hardware)
4. If re-init fails → display remains on (hardware default), but buzzer/LED feedback and touch reset become unavailable

**Recovery State Machine Integration**

| Trigger | Recovery Action | State Transition |
|---------|----------------|-----------------|
| PN532 timeout × 2 | SCL recovery → PN532 re-init | IDLE → IDLE (silent recovery) |
| PN532 re-init fails | Mark RFID unavailable | → ERROR (user notification) |
| CH422G timeout | SCL recovery → CH422G re-init | IDLE → IDLE (silent recovery) |
| Touch timeout × 5 | SCL recovery → log warning | No state change (touch auto-recovers) |
| SCL recovery fails | Full bus reset (Wire.end/begin) | Log critical warning |

**Preventive Measures**

- **Transaction serialization:** All I2C access (touch polling, RFID operations, CH422G writes) must go through a single task or be mutex-protected. No concurrent I2C transactions.
- **Inter-device delay:** 1 ms minimum gap between transactions to different devices, allowing bus settle time.
- **Watchdog:** If I2C bus is unrecoverable after 3 consecutive full recovery attempts, log error and continue operating with degraded functionality (display-only mode, no RFID/buzzer/LED).

---

## 14. Constraints

### 14.1 Memory Budget

**PSRAM (8 MB OPI) — shared resource:**

| Consumer | Steady-State | Peak (during DB update) | Notes |
|----------|-------------|------------------------|-------|
| LVGL display buffer | ~768 KB | ~768 KB | `(800×480) / 10 × 2 bytes` = 76,800 pixels × 2 = ~150 KB per buffer; single buffer used |
| FilamentProfile cache | ~100 KB | ~100 KB | ~500 profiles × ~200 bytes each |
| SpoolRecord cache | ~20 KB | ~20 KB | ~100 spools × ~200 bytes each |
| ArduinoJson document (DB load) | 0 | ~600 KB | Transient: raw JSON (~180 KB) + JsonDocument overhead (~3× raw). Freed after parse. |
| HTTP download buffer | 0 | ~512 KB | Raw JSON streamed into PSRAM buffer before hash + schema validation |
| Gzip compression buffer | 0 | ~100 KB | zlib deflate working memory during gzip-to-LittleFS write |
| LVGL objects (all screens) | ~200 KB | ~200 KB | Widgets, styles, screen trees |
| **Total** | **~1.1 MB** | **~2.3 MB** | **Peak is ~29% of 8 MB** |

Headroom at peak: ~5.7 MB free. This is comfortable but must be monitored if features grow.

**Internal SRAM (~512 KB):**

| Consumer | Size | Notes |
|----------|------|-------|
| Stack (main task) | ~8 KB | Arduino default |
| LVGL core (timers, event queue) | ~30 KB | Estimate |
| WiFi/TCP stack (when active) | ~50 KB | ESP-IDF WiFi buffers |
| FreeRTOS heap overhead | ~20 KB | Task control blocks, queues |
| Application globals | ~10 KB | State machine, config struct, global instances |
| **Total** | **~118 KB** | **~23% of 512 KB** |

SRAM is primarily consumed by the networking stack during DB updates. Most large allocations (JSON, caches, display buffers) go to PSRAM.

### 14.2 Hard Limits

**Storage & Collection Limits:**

| Resource | Limit | Enforced By | Failure Behavior |
|----------|-------|-------------|------------------|
| Raw material database JSON | **512 KB max** | Check `Content-Length` header before download; reject if exceeded | Error: "Database too large ({size} KB, max 512 KB). Update aborted." Previous DB preserved. |
| Gzipped database on LittleFS | **80 KB max** | Check compressed size after gzip | Error: "Compressed database exceeds storage budget." |
| Inventory spool count | **100 spools max** | `addSpool()` checks count before insert | Error: "Inventory full (100 spools). Archive or delete unused spools." |
| Inventory JSON on LittleFS | **64 KB max** | Check serialized size before atomic write | Error: "Inventory file too large. Reduce weight history or archive spools." |
| Weight history per spool | **10 entries max** (in JSON) | `updateWeight()` trims oldest entries | Oldest entry removed; full history in daily SD logs (Section 5.9) |
| FilamentProfile cache | **1000 profiles max** | Reject load if `result.list` exceeds limit | Warning: "Database has {n} profiles, loading first 1000." |

**String Length Limits:**

| Field | Max Length | Constrained By | Truncation / Handling |
|-------|-----------|----------------|----------------------|
| Brand name (FilamentProfile) | **32 chars** | JSON parse: truncate on load | UI dropdown: `lv_label_set_long_mode(LV_LABEL_LONG_DOT)` adds "..." if too wide |
| Brand name (on RFID tag v2) | **12 chars** | Sector 12 field size (12 bytes ASCII, null-padded) | Truncated to 12 chars when writing to tag; full name preserved in inventory JSON |
| Filament name (FilamentProfile) | **48 chars** | JSON parse: truncate on load | UI label: `LV_LABEL_LONG_DOT` truncation. Main screen `labelName` width-limited. |
| Filament name (on RFID tag v2) | **16 chars** | Sector 13 field size (16 bytes ASCII, null-padded) | Truncated to 16 chars when writing to tag; full name preserved in inventory JSON |
| Material type | **5 chars** | CFS tag format: 5 chars, space-padded | Longer types (e.g., "PLA-Silk") truncated to 5 chars on tag ("PLA-S"); full type in inventory. `find_dropdown_option_index()` uses prefix match to handle truncated types. |
| Color name | **7 chars** | Hex format `#RRGGBB` | Always formatted as `#RRGGBB` (7 chars). Named colors use the hex string on tag. |
| Spool ID | **8 chars** | Format: `SPL-NNNN` | Auto-generated, sequential. Wraps at SPL-9999 (reuses archived IDs). |
| Tag UID | **20 chars** | Format: `XX:XX:XX:XX:XX:XX:XX` (7-byte UID, colon-separated hex) | Fixed format from MIFARE tag. |

**Numeric Value Limits:**

| Field | Min | Max | Unit | Enforced By | Notes |
|-------|-----|-----|------|-------------|-------|
| Weight (slider) | 0 | **5000** | grams | `lv_slider_set_range()` | Current code uses 0-1000; FSD specifies 0-5000 for large spools (e.g., 3-5 kg rolls). UI shows `"{n}g"`. |
| Weight (tag storage) | 0 | 9999 | mm (as length) | 4-digit field, zero-padded | `length = 330 * weight / 1000`. Max weight representable: `9999 * 1000 / 330 ≈ 30,300g` — well beyond practical spool sizes. |
| Nozzle temp | 0 | **500** | °C | Range validation on parse / custom entry | Reject values outside 0-500 as corrupt/invalid. |
| Bed temp | 0 | **150** | °C | Range validation on parse / custom entry | Reject values outside 0-150. |
| Print speed | 0 | **2000** | mm/s | Range validation on parse / custom entry | Reject values outside 0-2000. |
| Fan percent | 0 | **100** | % | `uint8_t` natural range + clamp | Values > 100 clamped to 100. |
| Diameter | **1000** | **3500** | µm | Range validation | Covers 1.0mm to 3.5mm. Common values: 1750 (1.75mm), 2850 (2.85mm). Values outside range treated as invalid. |
| Density | **0.50** | **5.00** | g/cm³ | Range validation | Covers all common filament materials (PLA ~1.24, ABS ~1.04, metal-fill ~3.5). |
| Color hex | 0x000000 | 0xFFFFFF | — | `uint32_t` masked to 24 bits | Upper byte ignored. |

### 14.3 PSRAM Allocation Failure

All large allocations use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. If PSRAM allocation returns `NULL`:

**During boot (DB or inventory load):**
1. Log error to Serial: "PSRAM alloc failed: {size} bytes for {purpose}"
2. Attempt fallback allocation from internal heap (smaller buffer, partial load)
3. If fallback also fails → show error on splash screen: "Memory error — database too large for available RAM"
4. System continues with empty DB/inventory (degraded mode: RFID read/write still works, but library is empty)

**During DB update (runtime):**
1. Abort download, free any partial buffers
2. Enter ERROR state: "Not enough memory to process database ({size} KB). Try restarting the device."
3. Previous database preserved (download hasn't touched LittleFS yet)
4. Retryable — user can restart device to reclaim fragmented memory, then retry

**During inventory save:**
1. Serialization to PSRAM buffer fails → attempt direct stream-to-file write (lower memory, slower)
2. If stream write also fails → ERROR state: "Cannot save inventory — memory error. Data is still in RAM cache but not persisted. Restart recommended."
3. Non-retryable in place (memory is exhausted); restart clears fragmentation

**Monitoring:**
- `heap_caps_get_free_size(MALLOC_CAP_SPIRAM)` logged at boot and after each major allocation
- If free PSRAM drops below **1 MB**, log a warning: "Low PSRAM: {free} KB remaining"
- Future: display memory usage in About screen

### 14.4 General Constraints

- LVGL object creation must occur after LVGL init
- No blocking operations in `loop()` (except WiFiManager captive portal, which is a known exception — see Section 11.9)
- LittleFS total budget ~100 KB: `material_database.json.gz` (~40-80 KB) + `inventory.json` (~50-64 KB) + `config.json` (<1 KB); see Section 5.7
- SD card is optional — system operates fully from LittleFS alone; SD adds backup, history logs, and export
- Extended v2 tag sectors (10-13) must not be written to tags that did not originate from this system

### 14.5 Network Security Model

#### Threat Model Assumption

**This system assumes a trusted local network.** All network communication (WiFi, HTTP to printer) occurs on the user's home/workshop LAN. There is no internet-facing attack surface — the device does not expose any server ports or accept inbound connections.

This assumption is documented explicitly because an attacker on the same LAN could:
- Spoof the printer IP and serve a malicious JSON payload
- Intercept the HTTP download (no encryption)
- Inject or modify data in transit (no integrity check beyond post-download SHA-256)

For typical home/workshop use this risk is acceptable. Users on shared or untrusted networks should isolate the printer and ESP32 on a dedicated VLAN or WiFi segment.

#### Creality K2 Plus Printer Security Posture

The Creality K2 Plus (and K2 Pro/Hi) provides **no authentication or encryption** on its local API:

| Aspect | Status |
|--------|--------|
| HTTPS/TLS | **Not supported.** All endpoints are plain HTTP (port 80). |
| API authentication | **None.** All HTTP requests are served without credentials. |
| WebSocket auth | **None.** Moonraker API is unauthenticated. |
| Web interfaces | Fluidd (port 4408), Mainsail (port 4409) — both unauthenticated |
| Camera feed | WebRTC on port 8000 — unauthenticated |
| Firmware base | Open-source Klipper + Moonraker stack |

The printer exposes full control to any device on the LAN. This is standard for consumer 3D printers and consistent with Klipper/Moonraker ecosystem norms. **There is no handshake, token, or certificate we can leverage for secure communication.**

#### Validation Hardening (Defense in Depth)

Since we cannot encrypt or authenticate the transport, we validate the response content to reduce the impact of spoofed or corrupted data:

| Check | Location | Purpose |
|-------|----------|---------|
| **Printer model validation** | `GET /info` response → `model` field | Only accept known models: `"F008"` (K2 Plus), `"F018"` (Hi), `"F028"` (K2 Pro). Reject unknown models with warning: "Unrecognized printer model '{model}'. Proceed anyway?" |
| **Content-Length cap** | HTTP response header | Reject downloads > 512 KB (Section 14.2). Prevents memory exhaustion from malicious oversized payload. |
| **JSON structure validation** | After download, before commit | Verify `result.list[]` array exists with expected `base`/`kvParam` sub-objects (Section 5.6, step 7). Reject structurally invalid JSON. |
| **SHA-256 hash** | Computed over raw JSON | Detects any modification between downloads. Stored in config for comparison (Section 8.2). |
| **Profile field bounds** | During FilamentDB parse | Reject profiles with out-of-range values: temperature < 0 or > 500°C, speed < 0 or > 2000 mm/s, diameter < 1000 µm or > 3500 µm, density < 0.5 or > 3.0 g/cm³. Log warning and skip invalid profile. |
| **String sanitization** | All string fields from JSON | Truncate to max field length. Strip non-printable characters (< 0x20 except whitespace). Prevents buffer overflows in fixed-size RFID tag fields and LVGL label rendering. |

#### Credentials Storage

| Credential | Storage | Notes |
|-----------|---------|-------|
| WiFi SSID/password | ESP32 NVS (non-volatile storage, flash-encrypted if enabled) | Managed by WiFi library, not by our code |
| Printer IP | `/config.json` on LittleFS (plaintext) | Not sensitive — LAN IP only |
| No API keys/tokens | N/A | Creality API is unauthenticated |

#### Future Security Improvements

- **mDNS/DNS-SD verification:** If Creality printers advertise via mDNS (e.g., `_http._tcp`), verify the service name matches before connecting. Adds a weak form of endpoint identity.
- **Response signing:** If Creality ever adds response signatures or checksums in headers, validate them.
- **HTTPS support:** If a future Creality firmware adds TLS, upgrade `HTTPClient` to use HTTPS with certificate pinning.
- **Network isolation guidance:** Add a setup guide recommending VLAN or guest network isolation for security-conscious users.

---

## 15. Implementation Phasing

Features are prioritized into three tiers: **P0** (must-have for data integrity — implement first), **P1** (core functionality — implement after P0), and **P2** (refinement — implement as time allows).

### 15.1 P0 — Data Integrity & Safety

These features protect against data loss and corruption. They must be implemented before any inventory or tag-write functionality ships.

| # | Feature | FSD Section | Depends On | Deliverable |
|---|---------|-------------|------------|-------------|
| P0.1 | **Inventory atomic save** | 6.9 | LittleFS | `write .tmp → fsync → remove old → rename` pattern in `InventoryManager::save()` |
| P0.2 | **CRC-32 definition** | 7.6 | rfid_driver | `computeCRC32()` using IEEE 802.3 polynomial; validate on read, recompute on write |
| P0.3 | **Mirror voting** | 7.8 | P0.2 | `readMirrors()` with 3-way comparison, majority selection, disagreement logging |
| P0.4 | **Database max size guard** | 14.2 | network_manager | `Content-Length` check (512 KB max) before download; reject oversized responses |
| P0.5 | **ERROR state semantics** | 12.4 | system_state, ui_manager | Error dialog with per-error timeout table, retry/dismiss behavior, `USER_RETRY`/`USER_DISMISS` events |
| P0.6 | **Tag ↔ inventory reconciliation** | 6.7 | P0.3, inventory_manager | Weight comparison on scan (±5g tolerance), prompt on mismatch, user chooses tag vs inventory value |
| P0.7 | **Unknown/foreign v2 tag handling** | 7.5 (origin detection) | P0.2, rfid_driver | Check origin magic (`0x4B324658`); refuse to overwrite sectors 10-13 on foreign tags; warn user |

**Implementation order:** P0.1 → P0.2 → P0.3 → P0.4 → P0.5 → P0.6 → P0.7

Rationale: Atomic save (P0.1) is standalone and protects all subsequent features. CRC (P0.2) and mirrors (P0.3) are prerequisites for tag reconciliation (P0.6) and foreign tag detection (P0.7). Error semantics (P0.5) provides the dialog framework used by reconciliation and foreign tag warnings.

### 15.2 P1 — Core Functionality

These deliver the primary user-facing features. Implement after P0 is solid.

| # | Feature | FSD Section | Depends On |
|---|---------|-------------|------------|
| P1.1 | **TagData struct & data model separation** | 3.2, 7.9 | P0.2, P0.3 |
| P1.2 | **Inventory manager** (CRUD, persistence) | 6.1–6.6 | P0.1, P1.1 |
| P1.3 | **Inventory screen** (list/grid, scan, filter) | 11.4 | P1.2 |
| P1.4 | **Spool detail screen** (view/edit weight, history) | 11.5 | P1.2 |
| P1.5 | **Custom spool entry** (multi-step form) | 11.6 | P1.2 |
| P1.6 | **Extended tag write** (v2 sectors 10-13) | 7.5 | P0.7, P1.1 |
| P1.7 | **UID collision handling** | 6.8 | P1.2 |
| P1.8 | **Archive vs delete** | 6.10 | P1.2 |
| P1.9 | **Operation lock & UI guards** | 12.5 | P0.5 |
| P1.10 | **Database update flow** (SHA-256 hash, schema validation) | 5.6 | P0.4 |
| P1.11 | **Feedback module** (buzzer + LEDs) | 9 | CH422G driver |

### 15.3 P2 — Refinement

Polish and resilience. Implement as time allows; system is functional without these.

| # | Feature | FSD Section | Depends On |
|---|---------|-------------|------------|
| P2.1 | I2C bus recovery (SCL pulse, PN532/CH422G reset) | 13.7 | rfid_driver, CH422G |
| P2.2 | PSRAM allocation failure handling | 14.3 | — |
| P2.3 | SD card backup & usage logging (CSV) | 5.7, 5.9 | sd_manager |
| P2.4 | Network security hardening (validation checks) | 14.5 | P1.10 |
| P2.5 | Memory budget monitoring (runtime heap logging) | 14.1 | — |
| P2.6 | String length enforcement (truncation on parse/display) | 14.2 | — |
| P2.7 | Battery monitoring & low-battery state | 16 (Future) | GPIO6 ADC (Sensor AD header) |
| P2.8 | Save atomicity for config.json | — | P0.1 pattern |

---

## 16. Future Extensions

- **Battery operation:** Portable use via 18650 / 3.7V LiPo cell connected to BAT1. The Waveshare 4.3C has full on-board charging and boost circuitry (CS8501). Software support needed:
  - Battery voltage monitoring via **GPIO6 Sensor AD** header (dev board: on-board ÷3 divider on J6; 4.3C: EXIO_ADC via R18/R19 divider)
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
- **Rich audio feedback (4.3C only):** Leverage ES8311/NS4150B audio subsystem for distinct tones, melodies, or voice prompts (dev board has no audio codec — uses external active buzzer only, see Section 9)

*Note: Filament database updates via printer HTTP API are implemented in current scope (see Section 5.6).*

---

## 17. Non-Goals (Current Phase)

- Printer motion control
- G-code generation
- Real-time printer telemetry
- Cloud account management
- Multi-user access control

---

## 18. Status

**Document Status:** Draft (v2.2)

This FSD reflects the planned architecture for the filament inventory management system, extending the original RFID read/write tool with spool cataloging, usage tracking, custom spool creation, dual-storage architecture (LittleFS + SD card), and manual filament database updates from Creality printers via HTTP.

v2.2 adds implementation phasing (Section 15), formal hard limits for all string/numeric fields (Section 14.2), TagData struct and data model separation (Sections 3.2, 7.9), and I2C bus recovery procedures (Section 13.7).
