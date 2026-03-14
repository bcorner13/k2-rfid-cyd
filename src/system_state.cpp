#include "system_state.h"

StateMachine sysState;

StateMachine::StateMachine() : currentState(SystemState::BOOT) {
    _errorCtx.failed_state = SystemState::IDLE;
    _errorCtx.auto_dismiss_ms = 0;
    _errorCtx.retryable = false;
}

void StateMachine::init() {
    transitionTo(SystemState::IDLE);
}

SystemState StateMachine::getCurrentState() const {
    return currentState;
}

const char* StateMachine::getStateName() const {
    switch (currentState) {
        case SystemState::BOOT:               return "BOOT";
        case SystemState::IDLE:               return "IDLE";
        case SystemState::READING_TAG:        return "READING";
        case SystemState::WRITING_TAG:        return "WRITING";
        case SystemState::VERIFYING_TAG:      return "VERIFYING";
        case SystemState::SCANNING_INVENTORY: return "SCANNING";
        case SystemState::EDITING_SPOOL:      return "EDITING";
        case SystemState::CUSTOM_ENTRY:       return "CUSTOM ENTRY";
        case SystemState::UPDATING_DATABASE:  return "UPDATING DB";
        case SystemState::WIFI_CONFIG:        return "WIFI CONFIG";
        case SystemState::ERROR:              return "ERROR";
        case SystemState::LOW_BATTERY:        return "LOW BATT";
        case SystemState::SLEEP:              return "SLEEP";
        default:                              return "UNKNOWN";
    }
}

void StateMachine::enterError(const String& message, uint16_t autoDismissMs, bool retryable) {
    _errorCtx.failed_state = currentState;
    _errorCtx.message = message;
    _errorCtx.auto_dismiss_ms = autoDismissMs;
    _errorCtx.retryable = retryable;
    _errorEnteredMs = millis();
    transitionTo(SystemState::ERROR);
}

bool StateMachine::tick() {
    if (currentState != SystemState::ERROR) return false;
    if (_errorCtx.auto_dismiss_ms == 0) return false;
    if (millis() - _errorEnteredMs < _errorCtx.auto_dismiss_ms) return false;
    handleEvent(SystemEvent::TIMEOUT);
    return true;
}

void StateMachine::transitionTo(SystemState newState) {
    currentState = newState;
    Serial.printf("State: -> %s\n", getStateName());
}

// FSD Section 12.3: Key Transitions
void StateMachine::handleEvent(SystemEvent event) {
    switch (currentState) {
        case SystemState::BOOT:
            if (event == SystemEvent::INIT_DONE)
                transitionTo(SystemState::IDLE);
            break;

        case SystemState::IDLE:
            switch (event) {
                case SystemEvent::SCAN_REQUEST:
                    transitionTo(SystemState::SCANNING_INVENTORY);
                    break;
                case SystemEvent::READ_REQUEST:
                    transitionTo(SystemState::READING_TAG);
                    break;
                case SystemEvent::WRITE_REQUEST:
                    transitionTo(SystemState::WRITING_TAG);
                    break;
                case SystemEvent::EDIT_REQUEST:
                    transitionTo(SystemState::EDITING_SPOOL);
                    break;
                case SystemEvent::CUSTOM_REQUEST:
                    transitionTo(SystemState::CUSTOM_ENTRY);
                    break;
                case SystemEvent::DB_UPDATE_REQUEST:
                    transitionTo(SystemState::UPDATING_DATABASE);
                    break;
                case SystemEvent::WIFI_CONFIG_REQUEST:
                    transitionTo(SystemState::WIFI_CONFIG);
                    break;
                case SystemEvent::BATTERY_CRITICAL:
                    transitionTo(SystemState::LOW_BATTERY);
                    break;
                default:
                    break;
            }
            break;

        case SystemState::WIFI_CONFIG:
            if (event == SystemEvent::OPERATION_SUCCESS || event == SystemEvent::USER_DISMISS)
                transitionTo(SystemState::IDLE);
            else if (event == SystemEvent::OPERATION_FAILED)
                enterError("WiFi config failed", 0, true);
            break;

        case SystemState::SCANNING_INVENTORY:
            if (event == SystemEvent::TAG_DETECTED)
                transitionTo(SystemState::READING_TAG);
            else if (event == SystemEvent::OPERATION_FAILED)
                enterError("No tag detected", 5000, true);
            break;

        case SystemState::READING_TAG:
            if (event == SystemEvent::OPERATION_SUCCESS)
                transitionTo(SystemState::IDLE);
            else if (event == SystemEvent::OPERATION_FAILED)
                enterError("Tag read failed", 5000, true);
            break;

        case SystemState::WRITING_TAG:
            if (event == SystemEvent::OPERATION_SUCCESS)
                transitionTo(SystemState::VERIFYING_TAG);
            else if (event == SystemEvent::OPERATION_FAILED)
                enterError("Tag write failed", 10000, true);
            break;

        case SystemState::VERIFYING_TAG:
            if (event == SystemEvent::OPERATION_SUCCESS)
                transitionTo(SystemState::IDLE);
            else if (event == SystemEvent::OPERATION_FAILED)
                enterError("Tag verification failed", 10000, true);
            break;

        case SystemState::EDITING_SPOOL:
            if (event == SystemEvent::SAVE_REQUEST)
                transitionTo(SystemState::IDLE);
            else if (event == SystemEvent::OPERATION_FAILED)
                enterError("Failed to save spool data", 0, false);
            break;

        case SystemState::CUSTOM_ENTRY:
            if (event == SystemEvent::SAVE_REQUEST)
                transitionTo(SystemState::IDLE);
            else if (event == SystemEvent::OPERATION_FAILED)
                enterError("Failed to save custom spool", 0, false);
            break;

        case SystemState::UPDATING_DATABASE:
            if (event == SystemEvent::DB_UPDATE_SUCCESS)
                transitionTo(SystemState::IDLE);
            else if (event == SystemEvent::DB_UPDATE_FAILED)
                enterError("Database update failed", 15000, true);
            break;

        case SystemState::ERROR:
            if (event == SystemEvent::USER_DISMISS || event == SystemEvent::TIMEOUT)
                transitionTo(SystemState::IDLE);
            else if (event == SystemEvent::USER_RETRY) {
                // Return to the failed state for retry
                transitionTo(_errorCtx.failed_state);
            }
            break;

        case SystemState::LOW_BATTERY:
            if (event == SystemEvent::WAKE_UP)
                transitionTo(SystemState::IDLE);
            break;

        default:
            break;
    }
}
