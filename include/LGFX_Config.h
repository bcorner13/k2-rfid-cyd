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
#include <lgfx/v1/misc/Panel_Device.hpp>
#include <lgfx/v1/touch/Touch_GT911.hpp>

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
            cfg.pin_d0  = 8;   // B0
            cfg.pin_d1  = 3;   // B1
            cfg.pin_d2  = 46;  // B2
            cfg.pin_d3  = 9;   // B3
            cfg.pin_d4  = 1;   // B4

            // Green channel — G0(LSB)..G5(MSB)
            cfg.pin_d5  = 5;   // G0
            cfg.pin_d6  = 6;   // G1
            cfg.pin_d7  = 7;   // G2
            cfg.pin_d8  = 15;  // G3
            cfg.pin_d9  = 16;  // G4
            cfg.pin_d10 = 4;   // G5

            // Red channel — R0(LSB)..R4(MSB)
            cfg.pin_d11 = 45;  // R0
            cfg.pin_d12 = 48;  // R1
            cfg.pin_d13 = 47;  // R2
            cfg.pin_d14 = 21;  // R3
            cfg.pin_d15 = 14;  // R4

            // Sync/control signals
            cfg.pin_henable = 40;  // DE
            cfg.pin_vsync   = 41;
            cfg.pin_hsync   = 39;
            cfg.pin_pclk    = 42;

            // 16 MHz per Makerfabs official code. Spec typical is 30 MHz (24-bit mode);
            // lower is safe on ESP32-S3 RGB peripheral.
            cfg.freq_write = 16000000;

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
            cfg.pin_bl      = 2;
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
            cfg.pin_sda  = 17;
            cfg.pin_scl  = 18;
            cfg.pin_int  = -1;   // INT not connected on MaTouch
            cfg.pin_rst  = 38;   // RST on GPIO38 (V1.3); V1.1 used a different pin
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
