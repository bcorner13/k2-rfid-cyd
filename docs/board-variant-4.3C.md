# Board Variants: ESP32-S3-Touch-LCD-4.3 (Dev) vs 4.3C (Production)

> **This project targets the 4.3 development board.** This document describes the differences between the two variants and serves as a reference if adapting to the 4.3C.

---

## How to identify your board

| Check | 4.3 (Development) | 4.3C (Production / AI Voice) |
|-------|-------------------|------------------------------|
| **Silkscreen** | "ESP32-S3-Touch-LCD-4.3" | "ESP32-S3-Touch-LCD-4.3C" |
| **RS-485 terminal** | Present (SP3485, IO15/IO16) | Not present |
| **CAN bus terminal** | Present (TJA1051T) | Not present |
| **Audio codec** | None | ES8311 + ES7210 + NS4150B speaker amp |
| **RTC** | None | PCF85063A |
| **Optocoupler I/O (P1)** | None | DOUT0/DOUT1 + DIN0/DIN1 |
| **Sensor AD header (J6)** | GPIO6 (ADC1_CH5) with ÷3 divider | Not present (GPIO6 = I2S_MCLK) |
| **Schematic** | `docs/hardware/dev-board-4.3/ESP32-S3-Touch-LCD-4.3-Sch.pdf` | `docs/hardware/production-board-4.3C/ESP32-S3-Touch-LCD-4.3C-Schematics.pdf` |

## What they share

Both variants use the same core hardware:

- **MCU:** ESP32-S3-WROOM-1 (N16R8 — 16 MB flash, 8 MB PSRAM)
- **Display:** 800×480 RGB LCD (ST7262), 16-bit parallel bus at 12 MHz
- **Touch:** GT911 capacitive (I2C, GPIO8 SDA / GPIO9 SCL)
- **I2C Expander:** CH422G (U10) — EXIO1 = TP_RST, EXIO2 = DISP, EXIO4 = SD_CS
- **Backlight:** Controlled via CH422G EXIO2 (DISP on/off), not a direct ESP32 GPIO
- **USB-to-UART:** CH343P (upload/monitor port)
- **Battery charger:** CS8501

The LGFX display bus configuration (`include/LGFX_Config.h`) is identical for both variants — the RGB data pins, HSYNC/VSYNC/DE/PCLK, and GT911 touch all use the same GPIOs.

---

## Official documentation

Waveshare does not publish a separate wiki for "4.3C"; the **4.3** wiki covers the hardware family.

| Resource | URL | Notes |
|----------|-----|--------|
| **Waveshare wiki** | [ESP32-S3-Touch-LCD-4.3](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-4.3) | Pinout, CH422G usage, Arduino/ESP-IDF demos, LVGL, FAQ |
| **Dev board schematic** | `docs/hardware/dev-board-4.3/ESP32-S3-Touch-LCD-4.3-Sch.pdf` | Primary reference for this project |
| **Production schematic** | `docs/hardware/production-board-4.3C/ESP32-S3-Touch-LCD-4.3C-Schematics.pdf` | Reference only |
| **CH422G datasheet** | `docs/hardware/datasheets/CH422DS1_EN.pdf` | I2C IO expander |
| **ESP LCD FAQ (drift)** | [Why do I get drift…](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/lcd.html#why-do-i-get-drift-overall-drift-of-the-display-when-esp32-s3-is-driving-an-rgb-lcd-screen) | RGB screen shake/drift: PCLK, PSRAM, bounce buffer |
| **Waveshare demo pack** | [ESP32-S3-Touch-LCD-4.3-Demo.zip](https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-4.3/ESP32-S3-Touch-LCD-4.3-Demo.zip) | Arduino + ESP-IDF demos |

---

## CH422G (U10) setup

Both variants use CH422G for display/touch control. This project uses **LovyanGFX** and **LVGL 9** (not Waveshare's ESP32_Display_Panel / LVGL 8.x), but follows the same CH422G init pattern as the official demos.

1. **Add the IO expander library**
   Use [ESP32_IO_Expander](https://github.com/esp-arduino-libs/ESP32_IO_Expander) (Espressif). Supports CH422G with Arduino/PlatformIO. IO0–IO7 = EXIO0–EXIO7; pins 8–11 = OC outputs.

2. **Init CH422G before the display**
   Before calling `gfx.begin()`:
   - Create `esp_expander::CH422G` with SCL=9, SDA=8, address `ESP_IO_EXPANDER_I2C_CH422G_ADDRESS`.
   - Call `init()` then `begin()`. Default `begin()` sets IO0–IO7 as output high, so **EXIO1 (TP_RST)** and **EXIO2 (DISP)** go high — touch out of reset and backlight on.
   - Optionally drive a touch-reset sequence (TP_RST low → delay → high) before starting the panel.

3. **Keep LGFX config as-is**
   `LGFX_Config.h` stays unchanged (LovyanGFX Bus_RGB + Panel_RGB + Touch_GT911, no backlight GPIO). Only the CH422G init is aligned with the official demos.

---

## Dev board specifics (4.3)

Features available on the dev board that are **not** on the 4.3C:

- **RS-485** (SP3485): IO15 (TX) / IO16 (RX) — usable as general GPIO when RS-485 terminal is disconnected
- **CAN bus** (TJA1051T): Directly connected to ESP32-S3 TWAI peripheral
- **USB host switch** (FSUSB42UMX): EXIO5 selects between USB-to-UART and USB host
- **Sensor AD** (J6 header): GPIO6 / ADC1_CH5 with on-board ÷3 voltage divider — used for battery monitoring
- **CH422G OC outputs** (OC0–OC3): Open-collector outputs available for feedback hardware (buzzer, LEDs)

## 4.3C-only features (not available on dev board)

- **Audio:** ES8311 DAC + ES7210 ADC + NS4150B speaker amplifier (I2S on GPIO6/GPIO15/GPIO16)
- **RTC:** PCF85063A (I2C, shared bus)
- **Optocoupler I/O:** DOUT0/DOUT1 digital outputs, DIN0/DIN1 digital inputs via P1 header
- **EXIO_ADC:** Battery voltage via CH422G (instead of GPIO6 Sensor AD)

---

## Adapting between variants

If porting this project to the 4.3C:

1. **GPIO6 conflict:** GPIO6 is I2S_MCLK on 4.3C (audio), not Sensor AD. Battery monitoring must use EXIO_ADC instead.
2. **IO15/IO16 conflict:** These are I2S BCLK/LRCK on 4.3C (audio), not RS-485. Cannot use as general GPIO.
3. **Feedback hardware:** Use P1 header DOUT0/DOUT1 (optocouplers) instead of CH422G OC0–OC3.
4. **RTC available:** Can use PCF85063A for timestamps instead of NTP.
5. **I2C bus:** 6 devices on 4.3C (GT911, CH422G, PN532, ES8311, ES7210, PCF85063A) vs 3 on dev (GT911, CH422G, PN532). May need lower bus speed or careful arbitration.
