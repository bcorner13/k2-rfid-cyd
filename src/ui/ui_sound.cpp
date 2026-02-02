/*
#include "ui/ui_sound.h"

UISound uiSound;

void UISound::init(int pin) {
    buzzerPin = pin;
    if (buzzerPin >= 0) {
        pinMode(buzzerPin, OUTPUT);
        digitalWrite(buzzerPin, LOW);
    }
}

void UISound::playClick() {
    if (buzzerPin < 0) return;
    // Short, high-pitched "tick" sound
    tone(buzzerPin, 2000, 20); 
}

void UISound::playTone(int freq, int durationMs) {
    if (buzzerPin < 0) return;
    tone(buzzerPin, freq, durationMs);
}

void UISound::playStartup() {
    if (buzzerPin < 0) return;
    tone(buzzerPin, 1000, 100);
    delay(100);
    tone(buzzerPin, 1500, 100);
    delay(100);
    tone(buzzerPin, 2000, 200);
}
*/