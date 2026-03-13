#pragma once
#include <Arduino.h>

// FSD Section 12.1
enum class SystemState {
    // Core states
    BOOT,
    IDLE,

    // RFID operations
    READING_TAG,
    WRITING_TAG,
    VERIFYING_TAG,

    // Inventory operations
    SCANNING_INVENTORY,
    EDITING_SPOOL,
    CUSTOM_ENTRY,

    // Network operations
    UPDATING_DATABASE,
    WIFI_CONFIG,

    // System states
    ERROR,
    LOW_BATTERY,
    SLEEP
};

// FSD Section 12.2
enum class SystemEvent {
    // Boot
    INIT_DONE,

    // RFID
    TAG_DETECTED,
    READ_REQUEST,
    WRITE_REQUEST,
    OPERATION_SUCCESS,
    OPERATION_FAILED,

    // Inventory
    SCAN_REQUEST,
    EDIT_REQUEST,
    CUSTOM_REQUEST,
    SAVE_REQUEST,

    // Network
    DB_UPDATE_REQUEST,
    DB_UPDATE_SUCCESS,
    DB_UPDATE_FAILED,
    WIFI_CONFIG_REQUEST,

    // Error recovery
    USER_DISMISS,
    USER_RETRY,

    // System
    BATTERY_CRITICAL,
    TIMEOUT,
    WAKE_UP
};

// FSD Section 12.4: Error context for retry/dismiss behavior
struct ErrorContext {
    SystemState failed_state;       // Which state failed
    String message;                 // Human-readable error description
    uint16_t auto_dismiss_ms;       // 0 = no auto-dismiss
    bool retryable;                 // false hides the Retry button
};

class StateMachine {
public:
    StateMachine();
    void init();
    void handleEvent(SystemEvent event);
    SystemState getCurrentState() const;
    const char* getStateName() const;

    // Error context access
    const ErrorContext& getErrorContext() const { return _errorCtx; }

    // Enter ERROR state with context (used by subsystems reporting failures)
    void enterError(const String& message, uint16_t autoDismissMs = 5000, bool retryable = true);

    // Check if system is busy (not IDLE)
    bool isBusy() const { return currentState != SystemState::IDLE; }

    // Call every loop() iteration; fires TIMEOUT if auto_dismiss_ms has elapsed.
    // Returns true if a TIMEOUT transition to IDLE just occurred (caller should refresh UI).
    bool tick();

private:
    SystemState currentState;
    ErrorContext _errorCtx;
    uint32_t _errorEnteredMs = 0;
    void transitionTo(SystemState newState);
};

extern StateMachine sysState;
