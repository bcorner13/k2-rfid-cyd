#ifndef LGFX_CONFIG_H_
#define LGFX_CONFIG_H_

/* Target board: Makerfabs MaTouch ESP32-S3 Parallel TFT 4.3" (SKU: E32S3RGB43).
 * No CH422G expander — backlight on GPIO2 (PWM), touch RST on GPIO38.
 * I2C bus: GPIO17 (SDA) / GPIO18 (SCL) — GT911 touch + Mabee I2C port (PN532).
 * Display: ST7262, 16-bit parallel RGB565 @ 12 MHz.
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
    lgfx::v1::Light_PWM   _light_instance;

public:
    LGFX(void)
    {
        { // Configure RGB bus — 16-bit parallel, RGB565
          // Pin order: d0=B0 … d4=B4, d5=G0 … d10=G5, d11=R0 … d15=R4
            auto cfg = _bus_instance.config();
            cfg.panel = &_panel_instance;

            // Blue channel (B[4:0])
            cfg.pin_d0  = 1;   // B0
            cfg.pin_d1  = 9;   // B1
            cfg.pin_d2  = 46;  // B2
            cfg.pin_d3  = 3;   // B3
            cfg.pin_d4  = 8;   // B4

            // Green channel (G[5:0])
            cfg.pin_d5  = 4;   // G0
            cfg.pin_d6  = 16;  // G1
            cfg.pin_d7  = 15;  // G2
            cfg.pin_d8  = 7;   // G3
            cfg.pin_d9  = 6;   // G4
            cfg.pin_d10 = 5;   // G5

            // Red channel (R[4:0])
            cfg.pin_d11 = 14;  // R0
            cfg.pin_d12 = 21;  // R1
            cfg.pin_d13 = 47;  // R2
            cfg.pin_d14 = 48;  // R3
            cfg.pin_d15 = 45;  // R4

            // Sync/control signals
            cfg.pin_henable = 40;  // DE
            cfg.pin_vsync   = 41;
            cfg.pin_hsync   = 39;
            cfg.pin_pclk    = 42;

            cfg.freq_write = 12000000;   // 12 MHz; lower reduces RGB jitter/drift (ESP FAQ)
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
            cfg.panel_width  = 800;
            cfg.panel_height = 480;
            cfg.memory_width  = 800;
            cfg.memory_height = 480;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            _panel_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        { // Backlight — GPIO2, PWM (V1.3); always-on on V2.0 unless R59 bridged
            auto cfg = _light_instance.config();
            cfg.pin_bl     = 2;
            cfg.invert     = false;
            cfg.freq       = 5000;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        { // Configure touch — GT911, I2C on GPIO17/18, RST on GPIO38
            auto cfg = _touch_instance.config();
            cfg.i2c_port = 0;
            cfg.i2c_addr = 0x5D;
            cfg.freq     = 100000;   // 100 kHz; more stable than 400 kHz on long wires
            cfg.pin_sda  = 17;
            cfg.pin_scl  = 18;
            cfg.pin_int  = -1;       // INT not connected on MaTouch
            cfg.pin_rst  = 38;
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
