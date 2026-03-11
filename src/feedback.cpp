#include <feedback.h>
#include <config_manager.h>

Feedback feedback;

void Feedback::init() {
#if FEEDBACK_HARDWARE_ENABLED
    if (FEEDBACK_PIN_BUZZER >= 0) {
        pinMode(FEEDBACK_PIN_BUZZER, OUTPUT);
        digitalWrite(FEEDBACK_PIN_BUZZER, LOW);
    }
    if (FEEDBACK_PIN_LED_GREEN >= 0) {
        pinMode(FEEDBACK_PIN_LED_GREEN, OUTPUT);
        digitalWrite(FEEDBACK_PIN_LED_GREEN, LOW);
    }
    if (FEEDBACK_PIN_LED_RED >= 0) {
        pinMode(FEEDBACK_PIN_LED_RED, OUTPUT);
        digitalWrite(FEEDBACK_PIN_LED_RED, LOW);
    }
    _initialized = true;
    Serial.printf("Feedback: init OK (buzzer=%d, green=%d, red=%d)\n",
                  FEEDBACK_PIN_BUZZER, FEEDBACK_PIN_LED_GREEN, FEEDBACK_PIN_LED_RED);
#else
    Serial.println("Feedback: hardware disabled (FEEDBACK_HARDWARE_ENABLED=0)");
#endif
}

void Feedback::update() {
    if (!_initialized) return;

    uint32_t now = millis();

    // Handle multi-beep pattern
    if (_beepCount > 0 && now >= _beepNextAt) {
        if (_beepPhaseOn) {
            // End of on-phase → turn off buzzer, start gap
            buzzerSet(false);
            _beepPhaseOn = false;
            _beepCount--;
            if (_beepCount > 0) {
                _beepNextAt = now + _beepOffMs;
            }
        } else {
            // End of gap → start next beep
            buzzerSet(true);
            _beepPhaseOn = true;
            _beepNextAt = now + _beepOnMs;
        }
    }

    // Handle single beep timeout
    if (_buzzerOn && _beepCount == 0 && _buzzerOffAt > 0 && now >= _buzzerOffAt) {
        buzzerSet(false);
        _buzzerOffAt = 0;
    }

    // Handle green LED timeout
    if (_greenOn && _greenOffAt > 0 && now >= _greenOffAt) {
        ledGreen(false);
        _greenOffAt = 0;
    }

    // Handle red LED timeout
    if (_redOn && _redOffAt > 0 && now >= _redOffAt) {
        ledRed(false);
        _redOffAt = 0;
    }
}

// --- High-level patterns (FSD Section 9.4) ---

void Feedback::readSuccess() {
    startBeepPattern(1, 100, 0);
    ledGreen(true);
    _greenOffAt = millis() + 1000;
}

void Feedback::writeSuccess() {
    startBeepPattern(2, 100, 100);
    ledGreen(true);
    _greenOffAt = millis() + 1500;
}

void Feedback::operationFailed() {
    startBeepPattern(1, 500, 0);
    ledRed(true);
    _redOffAt = millis() + 2000;
}

void Feedback::tagDetected() {
    startBeepPattern(1, 50, 0);
    ledGreen(true);
    _greenOffAt = millis() + 200;
}

void Feedback::spoolSaved() {
    startBeepPattern(1, 100, 0);
    ledGreen(true);
    _greenOffAt = millis() + 500;
}

void Feedback::dbUpdateSuccess() {
    startBeepPattern(2, 100, 100);
    ledGreen(true);
    _greenOffAt = millis() + 1000;
}

void Feedback::dbUpdateFailed() {
    startBeepPattern(1, 500, 0);
    ledRed(true);
    _redOffAt = millis() + 2000;
}

// --- Low-level control ---

void Feedback::beep(uint16_t duration_ms) {
    startBeepPattern(1, duration_ms, 0);
}

void Feedback::ledGreen(bool on) {
    _greenOn = on;
#if FEEDBACK_HARDWARE_ENABLED
    if (_initialized && FEEDBACK_PIN_LED_GREEN >= 0) digitalWrite(FEEDBACK_PIN_LED_GREEN, on ? HIGH : LOW);
#endif
}

void Feedback::ledRed(bool on) {
    _redOn = on;
#if FEEDBACK_HARDWARE_ENABLED
    if (_initialized && FEEDBACK_PIN_LED_RED >= 0) digitalWrite(FEEDBACK_PIN_LED_RED, on ? HIGH : LOW);
#endif
}

void Feedback::allOff() {
    buzzerSet(false);
    ledGreen(false);
    ledRed(false);
    _beepCount = 0;
    _buzzerOffAt = 0;
    _greenOffAt = 0;
    _redOffAt = 0;
}

// --- Private helpers ---

void Feedback::startBeepPattern(uint8_t count, uint16_t onMs, uint16_t offMs) {
    if (!config.data.beep_enabled) return;
    if (!_initialized) return;

    _beepCount = count;
    _beepOnMs = onMs;
    _beepOffMs = offMs;

    // Start first beep immediately
    buzzerSet(true);
    _beepPhaseOn = true;
    _beepNextAt = millis() + onMs;
}

void Feedback::buzzerSet(bool on) {
    _buzzerOn = on;
#if FEEDBACK_HARDWARE_ENABLED
    if (_initialized && FEEDBACK_PIN_BUZZER >= 0) digitalWrite(FEEDBACK_PIN_BUZZER, on ? HIGH : LOW);
#endif
}
