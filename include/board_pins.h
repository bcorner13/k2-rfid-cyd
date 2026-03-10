#pragma once
/**
 * @file board_pins.h
 * @brief Single source of truth for all GPIO assignments on the
 *        Makerfabs MaTouch ESP32-S3 Parallel TFT 4.3" (SKU: E32S3RGB43).
 *
 * Two hardware revisions exist. The version number is silkscreened on the PCB back.
 * Set -DBOARD_MATOUCH_V2 in build_flags for V2.0 boards.
 *
 * V1.3 default: backlight PWM on GPIO2, Mabee GPIO on GPIO19/20, no I2S.
 * V2.0 with -DBOARD_MATOUCH_V2: I2S audio on GPIO2/19/20, backlight always-on.
 */

// ─── RGB display bus ────────────────────────────────────────────────────────
// Source: Makerfabs wiki (Arduino_GFX v1.4.7 example). R0/G0/B0 = LSB.
#define PIN_LCD_DE      40
#define PIN_LCD_VSYNC   41
#define PIN_LCD_HSYNC   39
#define PIN_LCD_PCLK    42
// Red channel (R0=LSB → R4=MSB)
#define PIN_LCD_R0      45
#define PIN_LCD_R1      48
#define PIN_LCD_R2      47
#define PIN_LCD_R3      21
#define PIN_LCD_R4      14
// Green channel (G0=LSB → G5=MSB)
#define PIN_LCD_G0       5
#define PIN_LCD_G1       6
#define PIN_LCD_G2       7
#define PIN_LCD_G3      15
#define PIN_LCD_G4      16
#define PIN_LCD_G5       4
// Blue channel (B0=LSB → B4=MSB)
#define PIN_LCD_B0       8
#define PIN_LCD_B1       3
#define PIN_LCD_B2      46
#define PIN_LCD_B3       9
#define PIN_LCD_B4       1

// ─── Touch (GT911, I2C, V1.3+) ──────────────────────────────────────────────
#define PIN_TOUCH_SDA   17
#define PIN_TOUCH_SCL   18
#define PIN_TOUCH_RST   38
#define PIN_TOUCH_INT   -1   // Not connected on MaTouch

// ─── I2C bus (shared: GT911 + Mabee I2C + PCF8563 RTC + PN532) ─────────────
#define PIN_I2C_SDA     17
#define PIN_I2C_SCL     18

// ─── SD card (SPI) ───────────────────────────────────────────────────────────
#define PIN_SD_MOSI     11
#define PIN_SD_SCK      12
#define PIN_SD_MISO     13
#define PIN_SD_CS       10

// ─── Version-dependent pins ──────────────────────────────────────────────────
#if defined(BOARD_MATOUCH_V2)
    // V2.0: GPIO2/19/20 are I2S audio. Backlight is hardware always-on.
    // Solder R59 (+ optionally remove R29) to restore backlight PWM on V2.0.
    #define PIN_I2S_LRCLK    2
    #define PIN_I2S_DIN     19
    #define PIN_I2S_BCLK    20
    #define PIN_MABEE_GPIO1 -1   // Not available on V2.0 (used by I2S)
    #define PIN_MABEE_GPIO2 -1
    #define PIN_LCD_BL      -1   // Always-on; -1 disables Light_PWM init
#else
    // V1.3: GPIO2 = backlight PWM. GPIO19/20 = Mabee GPIO port.
    #define PIN_LCD_BL       2
    #define PIN_MABEE_GPIO1 19   // HY2.0-4P GPIO connector, pin 1
    #define PIN_MABEE_GPIO2 20   // HY2.0-4P GPIO connector, pin 2
    #define PIN_I2S_LRCLK   -1  // No I2S on V1.3
    #define PIN_I2S_DIN     -1
    #define PIN_I2S_BCLK    -1
#endif

// ─── Feedback hardware (active buzzer + status LEDs) ────────────────────────
// Wire to Mabee GPIO port (V1.3) — GPIO19 and GPIO20.
// On V2.0 these are I2S pins; use external expander or different GPIOs.
#define PIN_BUZZER      PIN_MABEE_GPIO1   // GPIO19 on V1.3
#define PIN_LED_GREEN   PIN_MABEE_GPIO2   // GPIO20 on V1.3
#define PIN_LED_RED     -1                // Assign to available GPIO if needed
