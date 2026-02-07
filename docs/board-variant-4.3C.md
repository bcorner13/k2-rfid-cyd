# Board variant: ESP32-S3-Touch-LCD-4.3C

This project targets the **4.3C** variant of the Waveshare ESP32-S3 Touch LCD 4.3" board.

## How to verify your board is the 4.3C

1. **Schematic**  
   The repo uses the **4.3C** schematic: `docs/ESP32-S3-Touch-LCD-4.3C-Schematics.pdf`.  
   If your board matches that schematic (e.g. U10 EXIO expander, EXIO_PWM backlight, IO8/IO9 I2C, EXIO2 = DISP), it is the 4.3C.

2. **Silkscreen / label**  
   Check the PCB or product label for a model marking such as **ESP32-S3-Touch-LCD-4.3C** or **4.3C**.

3. **Hardware details (4.3C)**  
   - Backlight: **EXIO_PWM** from U10 (I2C expander), not a direct ESP32 GPIO.  
   - Touch reset: **EXIO1** (CTP RST) via U10.  
   - Display control: **EXIO2** = DISP via U10.  
   - Shared I2C on **IO8 (SDA)** and **IO9 (SCL)** for touch (GT911) and U10.

If your board is 4.3 or 4.3B (different schematic, e.g. direct backlight GPIO), the LGFX and backlight notes in `include/LGFX_Config.h` may not apply; adjust from that board's schematic.

---

## Official documentation (how the board is supposed to work)

Waveshare does not publish a separate wiki page for "4.3C"; the **4.3** wiki describes the same hardware family (CH422G, RGB LCD, GT911 touch). Use it as the main reference.

| Resource | URL | Notes |
|----------|-----|--------|
| **Waveshare wiki** | [ESP32-S3-Touch-LCD-4.3](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-4.3) | Pinout, CH422G usage, Arduino/ESP-IDF demos, LVGL, FAQ |
| **Schematic (4.3)** | [ESP32-S3-Touch-LCD-4.3-Sch.pdf](https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-4.3/manual/ESP32-S3-Touch-LCD-4.3-Sch.pdf) | C variant may differ slightly; we use `docs/ESP32-S3-Touch-LCD-4.3C-Schematics.pdf` |
| **CH422G datasheet** | [CH422DS1_EN.pdf](https://files.waveshare.com/wiki/common/CH422DS1_EN.pdf) | I2C IO expander: EXIO1=TP_RST, EXIO2=DISP, EXIO4=SD_CS, EXIO5=USB_SEL |
| **ESP LCD FAQ (drift)** | [Why do I get drift…](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/lcd.html#why-do-i-get-drift-overall-drift-of-the-display-when-esp32-s3-is-driving-an-rgb-lcd-screen) | RGB screen shake/drift: PCLK, PSRAM, bounce buffer, `CONFIG_ESP32S3_DATA_CACHE_LINE_64B` |
| **Waveshare demo pack** | [ESP32-S3-Touch-LCD-4.3-Demo.zip](https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-4.3/ESP32-S3-Touch-LCD-4.3-Demo.zip) | Arduino + ESP-IDF demos; uses `ESP_IOExpander_CH422G`, `ESP_PanelBus_RGB`, GT911 |

From the wiki, the board is supposed to work as follows:

- **Display:** RGB LCD (800×480), driven via ESP32-S3 RGB pins; backlight **on/off** via CH422G **EXIO2 (DISP)** on I2C (GPIO8/9). No direct PWM backlight on the ESP32 for this variant.
- **Touch:** GT911 on I2C (GPIO8 SDA, GPIO9 SCL); reset via CH422G **EXIO1 (TP_RST)**. TF card CS = **EXIO4 (SD_CS)**.
- **Libraries (Arduino):** `ESP32_Display_Panel`, `ESP32_IO_Expander` (CH422G); LVGL 8.4 in demos.
- **Screen drift:** Waveshare points to the [ESP LCD FAQ](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/lcd.html#why-do-i-get-drift-overall-drift-of-the-display-when-esp32-s3-is-driving-an-rgb-lcd-screen) for "screen drifting" (relevant to our "screen shake" issue: PCLK, PSRAM bandwidth, bounce buffer, cache line size).

---

## Adapting the official approach

This project uses **LovyanGFX** and **LVGL 9** instead of Waveshare’s **ESP32_Display_Panel** and LVGL 8.x, but we can still use the same **CH422G (U10)** setup as the official demos for touch reset and backlight.

1. **Add the official IO expander library**  
   Use [ESP32_IO_Expander](https://github.com/esp-arduino-libs/ESP32_IO_Expander) (Espressif). It supports CH422G and works with Arduino/PlatformIO. Pinout: IO0–IO7 = EXIO0–EXIO7 (e.g. EXIO1 = TP_RST, EXIO2 = DISP); pins 8–11 = OC outputs.

2. **Init CH422G before the display**  
   Before calling `gfx.begin()`:
   - Create `esp_expander::CH422G` with SCL=9, SDA=8, address `ESP_IO_EXPANDER_I2C_CH422G_ADDRESS`.
   - Call `init()` then `begin()`. By default `begin()` sets IO0–IO7 as output high, so **EXIO1 (TP_RST)** and **EXIO2 (DISP)** go high — touch out of reset and backlight on.
   - Optionally drive a touch-reset sequence (TP_RST low → delay → high) before starting the panel.

3. **Keep our LGFX config**  
   Our `LGFX_Config.h` stays as-is (LovyanGFX Bus_RGB + Panel_RGB + Touch_GT911, no backlight GPIO). We do not use ESP32_Display_Panel; only the CH422G init is aligned with the official demos.

4. **Brightness (optional)**  
   EXIO_PWM (U10 pin 10) can be used for backlight dimming later if the CH422G driver or a small custom I2C layer supports it; for now DISP on/off via EXIO2 is enough.
