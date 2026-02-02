#ifndef LGFX_CONFIG_H_
#define LGFX_CONFIG_H_

#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
// #include <lgfx/v1/platforms/esp32/Light_PWM.hpp> // Removed Light_PWM include
#include <lgfx/v1/touch/Touch_GT911.hpp>

class LGFX : public lgfx::LGFX_Device
{
public:
    lgfx::v1::Bus_RGB   _bus_instance;
    lgfx::v1::Panel_RGB _panel_instance;
    // lgfx::v1::Light_PWM _light_instance; // Removed Light_PWM instance
    lgfx::v1::Touch_GT911 _touch_instance;

public:
    LGFX(void)
    {
        { // Configure RGB bus
            auto cfg = _bus_instance.config();
            cfg.panel = &_panel_instance;
            cfg.pin_d0 = 14;  // B3
            cfg.pin_d1 = 38;  // B4
            cfg.pin_d2 = 18;  // B5
            cfg.pin_d3 = 17;  // B6
            cfg.pin_d4 = 10;  // B7
            cfg.pin_d5 = 39;  // G2
            cfg.pin_d6 = 0;   // G3
            cfg.pin_d7 = 45;  // G4
            cfg.pin_d8 = 48;  // G5
            cfg.pin_d9 = 47;  // G6
            cfg.pin_d10 = 21; // G7
            cfg.pin_d11 = 1;  // R3
            cfg.pin_d12 = 2;  // R4
            cfg.pin_d13 = 42; // R5
            cfg.pin_d14 = 41; // R6
            cfg.pin_d15 = 40; // R7

            cfg.pin_henable = 5;
            cfg.pin_vsync = 3;
            cfg.pin_hsync = 46;
            cfg.pin_pclk = 7;

            cfg.freq_write = 16000000;
            cfg.hsync_polarity = 0;
            cfg.hsync_front_porch = 8;
            cfg.hsync_pulse_width = 4;
            cfg.hsync_back_porch = 8;
            cfg.vsync_polarity = 0;
            cfg.vsync_front_porch = 8;
            cfg.vsync_pulse_width = 4;
            cfg.vsync_back_porch = 8;
            cfg.pclk_active_neg = 1;
            _bus_instance.config(cfg);
        }

        { // Configure display panel
            auto cfg = _panel_instance.config();
            cfg.panel_width = 800;   // Correct width
            cfg.panel_height = 480;  // Correct height
            cfg.memory_width = 800;
            cfg.memory_height = 480;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            _panel_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        // Removed backlight configuration and instance completely
        // _panel_instance.setLight(&_light_instance); // This line is also removed

        { // Configure touch
            auto cfg = _touch_instance.config();
            cfg.i2c_port = 0;    // Try Port 0 (Standard for default Wire)
            cfg.i2c_addr = 0x5D; // Explicitly set address (Common for GT911)
            cfg.freq = 400000;   // Ensure 400kHz frequency
            cfg.pin_sda = 8;
            cfg.pin_scl = 9;
            cfg.pin_int = 4;
            cfg.pin_rst = -1;
            cfg.x_min = 0;
            cfg.x_max = 799; // Adjust touch area
            cfg.y_min = 0;
            cfg.y_max = 479; // Adjust touch area
            cfg.bus_shared = false;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }

        setPanel(&_panel_instance);
    }
};

#endif