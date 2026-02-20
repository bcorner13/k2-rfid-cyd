#pragma once
/**
 * @file feedback.h
 * @brief Non-blocking buzzer + LED feedback (FSD Section 9).
 *
 * Provides immediate physical feedback for RFID operations and system events.
 * Buzzer respects config.data.beep_enabled; LEDs are always active.
 * All patterns are non-blocking — update() must be called from loop().
 *
 * Default pin assignments (dev board Option B):
 *   Buzzer: GPIO 46 (or CH422G OC0 if wired)
 *   Green LED: GPIO 15 (RS-485 TX, safe when terminal disconnected)
 *   Red LED: GPIO 16 (RS-485 RX, safe when terminal disconnected)
 *
 * Change FEEDBACK_PIN_* defines to match your wiring.
 */

#include <Arduino.h>

// Pin assignments — change to match your wiring
#ifndef FEEDBACK_PIN_BUZZER
#define FEEDBACK_PIN_BUZZER  46   // GPIO for active buzzer (HIGH = on)
#endif
#ifndef FEEDBACK_PIN_LED_GREEN
#define FEEDBACK_PIN_LED_GREEN 15  // GPIO for green LED
#endif
#ifndef FEEDBACK_PIN_LED_RED
#define FEEDBACK_PIN_LED_RED   16  // GPIO for red LED
#endif

// Set to 0 to disable feedback hardware entirely (software-only build)
#ifndef FEEDBACK_HARDWARE_ENABLED
#define FEEDBACK_HARDWARE_ENABLED 1
#endif

class Feedback {
public:
    void init();
    void update();  // Call from loop() — handles timed pattern off

    // High-level pattern methods (FSD Section 9.4)
    void readSuccess();       // 1 short beep (100ms), green LED 1s
    void writeSuccess();      // 2 short beeps (100ms on/off/on), green LED 1.5s
    void operationFailed();   // 1 long beep (500ms), red LED 2s
    void tagDetected();       // 1 short beep (50ms), green flash 200ms
    void spoolSaved();        // 1 short beep (100ms), green LED 500ms
    void dbUpdateSuccess();   // 2 short beeps, green LED 1s
    void dbUpdateFailed();    // 1 long beep, red LED 2s

    // Low-level control
    void beep(uint16_t duration_ms);
    void ledGreen(bool on);
    void ledRed(bool on);
    void allOff();

private:
    bool _initialized = false;

    // Buzzer state
    bool _buzzerOn = false;
    uint32_t _buzzerOffAt = 0;

    // Multi-beep pattern
    uint8_t _beepCount = 0;       // remaining beeps in pattern
    uint16_t _beepOnMs = 0;       // on duration per beep
    uint16_t _beepOffMs = 0;      // gap between beeps
    uint32_t _beepNextAt = 0;     // next beep toggle time
    bool _beepPhaseOn = false;    // currently in on or off phase

    // LED state
    bool _greenOn = false;
    uint32_t _greenOffAt = 0;
    bool _redOn = false;
    uint32_t _redOffAt = 0;

    void startBeepPattern(uint8_t count, uint16_t onMs, uint16_t offMs);
    void buzzerSet(bool on);
};

extern Feedback feedback;
