#pragma once
/**
 * @file ui_sound.h
 * @brief Audio output for UI events.
 *
 * Hardware is version-dependent:
 *
 *   V1.3 — No I2S on board. Use a passive buzzer on Mabee GPIO1 (GPIO19)
 *           via tone() or ledc PWM. Init with pin = PIN_MABEE_GPIO1.
 *
 *   V2.0 — I2S DAC on GPIO2(LRCLK) / GPIO19(DIN) / GPIO20(BCLK).
 *           Compile with -DBOARD_MATOUCH_V2 to enable I2S path.
 *           Requires an I2S amplifier/DAC connected to the Mabee GPIO port
 *           (e.g., MAX98357A: DIN=GPIO19, BCLK=GPIO20, LRCLK=GPIO2).
 *
 * Both paths share the same public API so callers are version-agnostic.
 */

#include <Arduino.h>
#include "board_pins.h"

#if defined(BOARD_MATOUCH_V2)
#include <driver/i2s.h>

class UISound {
public:
    /**
     * @brief Initialise I2S audio (V2.0).
     * @param sampleRate  PCM sample rate in Hz (default 44100).
     */
    void init(int sampleRate = 44100);

    void playClick();
    void playTone(int freqHz, int durationMs);
    void playStartup();

private:
    bool _initialized = false;
    int  _sampleRate  = 44100;

    static constexpr i2s_port_t I2S_PORT = I2S_NUM_0;

    // Pins come from board_pins.h
    static constexpr int PIN_DIN   = PIN_I2S_DIN;
    static constexpr int PIN_BCLK  = PIN_I2S_BCLK;
    static constexpr int PIN_LRCLK = PIN_I2S_LRCLK;

    void writeSine(int freqHz, int durationMs, float amplitude = 0.5f);
};

#else  // V1.3 — passive buzzer via LEDC PWM

class UISound {
public:
    /**
     * @brief Initialise buzzer on the given GPIO (default: PIN_MABEE_GPIO1).
     * @param pin  GPIO connected to passive buzzer.
     */
    void init(int pin = PIN_MABEE_GPIO1);

    void playClick();
    void playTone(int freqHz, int durationMs);
    void playStartup();

private:
    int  _pin          = -1;
    bool _initialized  = false;
};

#endif  // BOARD_MATOUCH_V2

extern UISound uiSound;
