#ifndef LGFX_CONFIG_H_
#define LGFX_CONFIG_H_

/* Target board: Makerfabs MaTouch ESP32-S3 Parallel TFT 4.3" (SKU: E32S3RGB43).
 *
 * Display: 4300H40R10-V03 LCD module, panel IC HX8664/HX8264.
 *   Driven in 16-bit RGB565 mode via ESP32-S3 native RGB LCD peripheral (no
 *   separate controller chip). Panel is 24-bit capable; upper 8 bits of each
 *   channel are not connected on this board.
 *
 * I2C bus: GPIO17 (SDA) / GPIO18 (SCL) — GT911 touch + Mabee I2C port (PN532).
 * Touch RST: GPIO38. Touch INT: not connected (-1).
 *
 * Hardware versions:
 *   V1.3 — backlight PWM on GPIO2; Mabee GPIO on GPIO19/20. No I2S audio.
 *   V2.0 — backlight always-on (solder R59 to enable PWM, remove R29 if flicker);
 *           GPIO2=I2S_LRCLK, GPIO19=I2S_DIN, GPIO20=I2S_BCLK. No Mabee GPIO.
 *
 * Pin bit-order: R0/G0/B0 = LSB of each channel → d11/d5/d0 respectively.
 * Source: Makerfabs Wiki example code (Arduino_GFX_Library v1.4.7 variant).
 */

#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/touch/Touch_GT911.hpp>
#include "board_pins.h"

class LGFX : public lgfx::LGFX_Device
{
public:
    lgfx::v1::Bus_RGB     _bus_instance;
    lgfx::v1::Panel_RGB   _panel_instance;
    lgfx::v1::Touch_GT911 _touch_instance;
#if !defined(BOARD_MATOUCH_V2) // V2.0 backlight is hardware always-on
    lgfx::v1::Light_PWM   _light_instance;
#endif

public:
    LGFX(void)
    {
        { // Configure RGB bus — 16-bit parallel RGB565
          // d0=B0(LSB) … d4=B4(MSB), d5=G0(LSB) … d10=G5(MSB), d11=R0(LSB) … d15=R4(MSB)
          // Confirmed from Makerfabs wiki code: R0=45,R4=14; G0=5,G5=4; B0=8,B4=1
            auto cfg = _bus_instance.config();
            cfg.panel = &_panel_instance;

            // Blue channel — B0(LSB)..B4(MSB)
            cfg.pin_d0  = PIN_LCD_B0;
            cfg.pin_d1  = PIN_LCD_B1;
            cfg.pin_d2  = PIN_LCD_B2;
            cfg.pin_d3  = PIN_LCD_B3;
            cfg.pin_d4  = PIN_LCD_B4;

            // Green channel — G0(LSB)..G5(MSB)
            cfg.pin_d5  = PIN_LCD_G0;
            cfg.pin_d6  = PIN_LCD_G1;
            cfg.pin_d7  = PIN_LCD_G2;
            cfg.pin_d8  = PIN_LCD_G3;
            cfg.pin_d9  = PIN_LCD_G4;
            cfg.pin_d10 = PIN_LCD_G5;

            // Red channel — R0(LSB)..R4(MSB)
            cfg.pin_d11 = PIN_LCD_R0;
            cfg.pin_d12 = PIN_LCD_R1;
            cfg.pin_d13 = PIN_LCD_R2;
            cfg.pin_d14 = PIN_LCD_R3;
            cfg.pin_d15 = PIN_LCD_R4;

            // Sync/control signals
            cfg.pin_henable = PIN_LCD_DE;
            cfg.pin_vsync   = PIN_LCD_VSYNC;
            cfg.pin_hsync   = PIN_LCD_HSYNC;
            cfg.pin_pclk    = PIN_LCD_PCLK;

            // 12 MHz is safer for ESP32-S3 RGB peripheral to prevent drift/shakes
            // when PSRAM or I2C (touch) is active.
            cfg.freq_write = 12000000;

            cfg.hsync_polarity    = 0;
            cfg.hsync_front_porch = 8;
            cfg.hsync_pulse_width = 4;
            cfg.hsync_back_porch  = 8;
            cfg.vsync_polarity    = 0;
            cfg.vsync_front_porch = 8;
            cfg.vsync_pulse_width = 4;
            cfg.vsync_back_porch  = 8;
            cfg.pclk_active_neg   = 1;
            _bus_instance.config(cfg);
        }

        { // Configure display panel
            auto cfg = _panel_instance.config();
            cfg.panel_width   = 800;
            cfg.panel_height  = 480;
            cfg.memory_width  = 800;
            cfg.memory_height = 480;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            _panel_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

#if !defined(BOARD_MATOUCH_V2)
        { // Backlight — V1.3: GPIO2 PWM. On V2.0 this pin is I2S_LRCLK; skip.
          // V2.0: solder R59 to restore independent BL control; remove R29 if flicker.
            auto cfg = _light_instance.config();
            cfg.pin_bl      = PIN_LCD_BL;
            cfg.invert      = false;
            cfg.freq        = 5000;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }
#endif

        { // Configure touch — GT911, I2C on GPIO17/18, RST on GPIO38 (V1.3+)
            auto cfg = _touch_instance.config();
            cfg.i2c_port = 0;
            cfg.i2c_addr = 0x5D;
            cfg.freq     = 100000;
            cfg.pin_sda  = PIN_TOUCH_SDA;
            cfg.pin_scl  = PIN_TOUCH_SCL;
            cfg.pin_int  = PIN_TOUCH_INT;
            cfg.pin_rst  = PIN_TOUCH_RST;
            cfg.x_min = 0;
            cfg.x_max = 799;
            cfg.y_min = 0;
            cfg.y_max = 479;
            cfg.bus_shared = false;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }

        setPanel(&_panel_instance);
    }
};

#endif
