/**
 * @file ui_sound.cpp
 * @brief UISound implementation — version-dependent audio output.
 *
 * V1.3: Passive buzzer on Mabee GPIO1 (GPIO19) driven by LEDC PWM via tone().
 *       tone() on ESP32-S3 Arduino core uses LEDC internally; no manual channel
 *       management needed.
 *
 * V2.0: I2S DAC on GPIO2(LRCLK) / GPIO19(DIN) / GPIO20(BCLK).
 *       Generates 16-bit PCM sine waves and writes to I2S_NUM_0.
 *       Requires -DBOARD_MATOUCH_V2 build flag and a connected I2S amplifier
 *       (e.g., MAX98357A). Stereo output — same data on both L and R channels.
 *
 * compile-time selection mirrors include/ui/ui_sound.h.
 */

#include "ui/ui_sound.h"

// ─── V1.3: passive buzzer via tone() (LEDC-backed on ESP32-S3) ───────────────
#if !defined(BOARD_MATOUCH_V2)

UISound uiSound;

/**
 * @brief Attach LEDC output to the buzzer pin.
 * @param pin  GPIO connected to passive buzzer (default: PIN_MABEE_GPIO1 / GPIO19).
 */
void UISound::init(int pin) {
    _pin = pin;
    if (_pin < 0) return;
    // Drive low — ensures pin is in a known-off state before first tone().
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
    _initialized = true;
    Serial.printf("UISound (V1.3): init OK — buzzer pin=%d\n", _pin);
}

/**
 * @brief Short click tone — 2 kHz for 20 ms.
 */
void UISound::playClick() {
    if (!_initialized || _pin < 0) return;
    tone(_pin, 2000, 20);
}

/**
 * @brief Play an arbitrary tone frequency for a given duration.
 *        Blocking: returns after durationMs.
 */
void UISound::playTone(int freqHz, int durationMs) {
    if (!_initialized || _pin < 0) return;
    tone(_pin, freqHz, durationMs);
    delay(durationMs);   // tone() is non-blocking; caller needs to wait
}

/**
 * @brief Three-note ascending startup melody (blocking, ~500 ms total).
 */
void UISound::playStartup() {
    if (!_initialized || _pin < 0) return;
    tone(_pin, 880,  100); delay(130);   // A5
    tone(_pin, 1108, 100); delay(130);   // C#6
    tone(_pin, 1318, 200); delay(220);   // E6
}

// ─── V2.0: I2S DAC (MAX98357A or equivalent) ─────────────────────────────────
#else

#include <math.h>    // sinf, M_PI

UISound uiSound;

/**
 * @brief Initialise I2S_NUM_0 for stereo 16-bit PCM output.
 * @param sampleRate  PCM sample rate in Hz (default 44100).
 *
 * Pins from board_pins.h:
 *   DIN   = PIN_I2S_DIN   = GPIO19
 *   BCLK  = PIN_I2S_BCLK  = GPIO20
 *   LRCLK = PIN_I2S_LRCLK = GPIO2
 */
void UISound::init(int sampleRate) {
    _sampleRate = sampleRate;

    i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate          = (uint32_t)_sampleRate,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,   // stereo
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = 0,
        .dma_buf_count        = 8,
        .dma_buf_len          = 128,
        .use_apll             = false,
        .tx_desc_auto_clear   = true,
    };
    esp_err_t err = i2s_driver_install(I2S_PORT, &cfg, 0, nullptr);
    if (err != ESP_OK) {
        Serial.printf("UISound (V2.0): i2s_driver_install failed: %d\n", err);
        return;
    }

    i2s_pin_config_t pins = {
        .bck_io_num   = PIN_BCLK,
        .ws_io_num    = PIN_LRCLK,
        .data_out_num = PIN_DIN,
        .data_in_num  = I2S_PIN_NO_CHANGE,
    };
    err = i2s_set_pin(I2S_PORT, &pins);
    if (err != ESP_OK) {
        Serial.printf("UISound (V2.0): i2s_set_pin failed: %d\n", err);
        i2s_driver_uninstall(I2S_PORT);
        return;
    }

    i2s_zero_dma_buffer(I2S_PORT);
    _initialized = true;
    Serial.printf("UISound (V2.0): init OK — %d Hz, DIN=%d BCLK=%d LRCLK=%d\n",
                  _sampleRate, PIN_DIN, PIN_BCLK, PIN_LRCLK);
}

/**
 * @brief Short click — 2 kHz sine for 15 ms.
 */
void UISound::playClick() {
    writeSine(2000, 15, 0.4f);
}

/**
 * @brief Play an arbitrary tone frequency for a given duration (blocking).
 */
void UISound::playTone(int freqHz, int durationMs) {
    writeSine(freqHz, durationMs, 0.5f);
}

/**
 * @brief Three-note ascending startup melody (blocking, ~500 ms total).
 */
void UISound::playStartup() {
    if (!_initialized) return;
    writeSine(880,  100, 0.5f);  // A5
    delay(30);
    writeSine(1108, 100, 0.5f);  // C#6
    delay(30);
    writeSine(1318, 200, 0.5f);  // E6
}

/**
 * @brief Generate a sine wave and write to I2S (blocking).
 *
 * Produces stereo output — same sample written to both L and R channels.
 * Buffer is processed in 256-frame chunks to bound stack usage.
 *
 * @param freqHz     Tone frequency in Hz.
 * @param durationMs Duration in milliseconds.
 * @param amplitude  Peak amplitude, 0.0–1.0 (default 0.5).
 */
void UISound::writeSine(int freqHz, int durationMs, float amplitude) {
    if (!_initialized || freqHz <= 0) return;

    const int totalFrames = (_sampleRate * durationMs) / 1000;
    static constexpr int CHUNK = 256;   // frames per write (stack: 256 * 4 = 1 KB)
    int16_t buf[CHUNK * 2];             // stereo interleaved: L, R, L, R…
    size_t bytesWritten = 0;
    int framesWritten   = 0;

    // Phase accumulator — keeps continuity across chunks
    float phase = 0.0f;
    const float phaseInc = 2.0f * (float)M_PI * (float)freqHz / (float)_sampleRate;
    const float peak     = amplitude * 32767.0f;

    while (framesWritten < totalFrames) {
        int chunk = (totalFrames - framesWritten < CHUNK)
                    ? (totalFrames - framesWritten)
                    : CHUNK;

        for (int i = 0; i < chunk; i++) {
            int16_t sample = (int16_t)(peak * sinf(phase));
            buf[i * 2]     = sample;   // Left
            buf[i * 2 + 1] = sample;   // Right (same)
            phase += phaseInc;
            if (phase >= 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;
        }

        i2s_write(I2S_PORT,
                  buf,
                  chunk * 2 * sizeof(int16_t),
                  &bytesWritten,
                  portMAX_DELAY);
        framesWritten += chunk;
    }
}

#endif  // BOARD_MATOUCH_V2
