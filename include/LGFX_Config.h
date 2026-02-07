#ifndef LGFX_CONFIG_H_
#define LGFX_CONFIG_H_

/* Target board: Waveshare ESP32-S3-Touch-LCD-4.3C (see docs/board-variant-4.3C.md). */

#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
/* 4.3C backlight is EXIO_PWM from U10 (I2C expander), not direct GPIO – no Light_PWM */
#include <lgfx/v1/touch/Touch_GT911.hpp>

/*
 * 4.3C WROOM pinout (for reference):
 *   LCD I2C: IO8 = SDA, IO9 = SCL (shared with Audio/RTC; touch GT911 and U10 on same bus).
 *   Touch:   IO4 = CTP IRQ.  CTP RST = EXIO1 (via U10, not direct GPIO).
 *   Display: EXIO2 = DISP (backlight/display control via U10). EXIO_PWM = U10 pin 10 (brightness).
 */

class LGFX : public lgfx::LGFX_Device
{
public:
    lgfx::v1::Bus_RGB   _bus_instance;
    lgfx::v1::Panel_RGB _panel_instance;
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

            cfg.freq_write = 12000000;  /* 12 MHz; lower can reduce RGB jitter/drift (ESP FAQ) */
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

        /* Backlight on 4.3C: EXIO_PWM from U10 pin 10 (I2C expander), not ESP32 GPIO. See separate backlight control via U10 I2C. */

        { // Configure touch (I2C, separate from RGB display bus)
            auto cfg = _touch_instance.config();
            cfg.i2c_port = 0;
            cfg.i2c_addr = 0x5D;
            cfg.freq = 100000;   // 100kHz can be more stable than 400kHz with long wires
            cfg.pin_sda = 8;
            cfg.pin_scl = 9;
            cfg.pin_int = 4;
            cfg.pin_rst = -1;
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