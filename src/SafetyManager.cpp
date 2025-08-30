#include "./include/SafetyManager.hpp"
#include "./include/hal/McuHAL.hpp"
#include "./include/controllers/TempController.hpp"
#include "./include/controllers/MotionController.hpp"
#include "./include/controllers/EndstopController.hpp"
#include "./include/controllers/FanController.hpp"
#include "./include/StateMachine.hpp"
#include "./include/Pins.hpp"

#define IDLE_TIMEOUT_S 300
#define SLEEP_TIMEOUT_S 1800
#define DEEP_SLEEP_TIMEOUT_S 7200

namespace {
    uint16_t lastCommandTime_s = 0;

    struct ErrorState {
        bool active = false;
        uint16_t timestamp_s = 0;
        char reason[12] = {0};
    };

    struct PowerManager {
        SafetyManager::PowerState currentState = SafetyManager::ACTIVE;
        uint16_t stateEntered_s = 0;
        uint8_t flags = 0;
    };

    ErrorState currentError;
    PowerManager pm;
}

namespace SafetyManager {
    void init() {
        lastCommandTime_s = hal::millis() / 1000;
        pm.currentState = ACTIVE;
        pm.stateEntered_s = lastCommandTime_s;
        pm.flags = 0;
    }

    void update() {
        updatePowerManagement();
        TempController::updateThermalProtection();

        if (hasCriticalCondition()) {
            emergencyStop("TEMP_HIGH");
        }

        if (isAnyEndstopTriggered()) {
            emergencyStop("ENDSTOP");
            EndstopController::handle(10, nullptr);
        }
    }

    void notifyCommandReceived() {
        uint16_t now_s = hal::millis() / 1000;
        lastCommandTime_s = now_s;
        if (pm.currentState != ACTIVE) {
            wakeUp();
        }
    }

    void updatePowerManagement() {
        uint16_t now_s = hal::millis() / 1000;
        uint16_t inactiveTime_s = now_s - lastCommandTime_s;

        MachineState state = StateMachine::getState();
        if (state == MachineState::Printing || state == MachineState::Homing ||
            state == MachineState::Error) {
            return;
        }

        PowerState targetState = ACTIVE;
        if (inactiveTime_s >= DEEP_SLEEP_TIMEOUT_S) {
            targetState = DEEP_SLEEP;
        } else if (inactiveTime_s >= SLEEP_TIMEOUT_S) {
            targetState = SLEEP;
        } else if (inactiveTime_s >= IDLE_TIMEOUT_S) {
            targetState = IDLE;
        }

        if (targetState != pm.currentState) {
            enterPowerState(targetState);
        }
    }

    void enterPowerState(PowerState newState) {
        switch (newState) {
            case IDLE:
                hal::serial->println(F("IDLE MODE"));
                FanController::setSpeed(32);
                pm.flags = 0;
                break;
            case SLEEP:
                hal::serial->println(F("SLEEP MODE"));
                MotionController::emergencyStop();
                pm.flags |= 0x01;
                if (TempController::getTemperature() <= 25.0f) {
                    TempController::setTargetTemperature(0);
                    pm.flags |= 0x02;
                }
                FanController::setSpeed(16);
                break;
            case DEEP_SLEEP:
                hal::serial->println(F("DEEP SLEEP"));
                hal::writeDigital(PIN_HEATER, false);
                pm.flags |= 0x02;
                FanController::setSpeed(0);
                pm.flags |= 0x04;
                break;
            case ACTIVE:
                break;
        }
        pm.currentState = newState;
        pm.stateEntered_s = hal::millis() / 1000;
    }

    void wakeUp() {
        if (pm.currentState == ACTIVE) return;
        hal::serial->print(F("WAKE FROM "));
        hal::serial->println((int) pm.currentState);
        if (pm.flags & 0x01) {
            MotionController::init();
        }
        pm.currentState = ACTIVE;
        pm.stateEntered_s = hal::millis() / 1000;
        pm.flags = 0;
    }

    PowerState getPowerState() {
        return pm.currentState;
    }

    const char *getPowerStateName() {
        switch (pm.currentState) {
            case ACTIVE:
                return "Active";
            case IDLE:
                return "Idle";
            case SLEEP:
                return "Sleep";
            case DEEP_SLEEP:
                return "Deep";
            default:
                return "?";
        }
    }

    uint16_t getInactiveTime() {
        return (hal::millis() / 1000) - lastCommandTime_s;
    }

    void forcePowerState(PowerState state) {
        enterPowerState(state);
    }

    void emergencyStop(const char *reason) {
        if (pm.currentState != ACTIVE) {
            wakeUp();
        }
        currentError.active = true;
        currentError.timestamp_s = hal::millis() / 1000;
        strncpy(currentError.reason, reason, sizeof(currentError.reason) - 1);
        hal::serial->print(F("!!! "));
        hal::serial->println(reason);
        hal::writeDigital(PIN_HEATER, false);
        MotionController::emergencyStop();
        FanController::setSpeed(0);
        StateMachine::setState(MachineState::Error);
    }

    bool isInErrorState() {
        return currentError.active;
    }

    const char *getErrorReason() {
        return currentError.active ? currentError.reason : "";
    }

    uint32_t getErrorTimestamp() {
        return currentError.active ? (hal::millis() / 1000 - currentError.timestamp_s) * 1000UL : 0;
    }

    bool clearError() {
        if (!currentError.active) return false;

        currentError.active = false;
        currentError.timestamp_s = 0;
        currentError.reason[0] = '\0';

        if (StateMachine::getState() == MachineState::Error) {
            StateMachine::setState(MachineState::Idle);
            hal::serial->println(F("ERR CLEARED"));
            return true;
        }
        return false;
    }

    bool hasCriticalCondition() {
        return TempController::getTemperature() > 250.0f;
    }

    bool isAnyEndstopTriggered() {
        return StateMachine::getState() == MachineState::Printing &&
               EndstopController::isAnyTriggered();
    }
}