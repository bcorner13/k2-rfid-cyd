#pragma once

enum class SystemState {
    BOOT,
    IDLE,
    READING_TAG,
    WRITING_TAG,
    VERIFYING_TAG,
    ERROR,
    LOW_BATTERY,
    SLEEP
};

enum class SystemEvent {
    INIT_DONE,
    TAG_DETECTED,
    READ_REQUEST,
    WRITE_REQUEST,
    OPERATION_SUCCESS,
    OPERATION_FAILED,
    BATTERY_CRITICAL,
    TIMEOUT,
    WAKE_UP
};

class StateMachine {
public:
    StateMachine();
    void init();
    void handleEvent(SystemEvent event);
    SystemState getCurrentState() const;
    const char* getStateName() const;

private:
    SystemState currentState;
    void transitionTo(SystemState newState);
};

extern StateMachine sysState;