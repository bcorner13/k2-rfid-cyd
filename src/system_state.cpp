#include "system_state.h"
#include <Arduino.h>

StateMachine sysState;

StateMachine::StateMachine() : currentState(SystemState::BOOT) {}

void StateMachine::init() {
    // Initial setup logic if needed
    transitionTo(SystemState::IDLE);
}

SystemState StateMachine::getCurrentState() const {
    return currentState;
}

const char* StateMachine::getStateName() const {
    switch (currentState) {
        case SystemState::BOOT: return "BOOT";
        case SystemState::IDLE: return "IDLE";
        case SystemState::READING_TAG: return "READING";
        case SystemState::WRITING_TAG: return "WRITING";
        case SystemState::VERIFYING_TAG: return "VERIFYING";
        case SystemState::ERROR: return "ERROR";
        case SystemState::LOW_BATTERY: return "LOW BATT";
        case SystemState::SLEEP: return "SLEEP";
        default: return "UNKNOWN";
    }
}

void StateMachine::transitionTo(SystemState newState) {
    // Exit logic for current state

    currentState = newState;
    Serial.printf("State Transition: -> %s\n", getStateName());

    // Entry logic for new state
    switch (newState) {
        case SystemState::ERROR:
            // Trigger Error Sound/LED
            break;
        case SystemState::LOW_BATTERY:
            // Disable high power peripherals
            break;
        default:
            break;
    }
}

void StateMachine::handleEvent(SystemEvent event) {
    switch (currentState) {
        case SystemState::BOOT:
            if (event == SystemEvent::INIT_DONE) transitionTo(SystemState::IDLE);
            break;

        case SystemState::IDLE:
            if (event == SystemEvent::TAG_DETECTED) transitionTo(SystemState::READING_TAG);
            if (event == SystemEvent::BATTERY_CRITICAL) transitionTo(SystemState::LOW_BATTERY);
            break;

        case SystemState::READING_TAG:
            if (event == SystemEvent::OPERATION_SUCCESS) transitionTo(SystemState::IDLE); // Or Show Data
            if (event == SystemEvent::OPERATION_FAILED) transitionTo(SystemState::ERROR);
            break;

        case SystemState::ERROR:
            if (event == SystemEvent::TIMEOUT) transitionTo(SystemState::IDLE);
            break;

        // ... Implement other transitions
        default:
            break;
    }
}