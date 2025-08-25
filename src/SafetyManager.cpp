#include <Arduino.h>
#include "./include/SafetyManager.hpp"
#include "./include/controllers/TempController.hpp"
#include "./include/controllers/MotionController.hpp"
#include "./include/controllers/EndstopController.hpp"
#include "./include/StateMachine.hpp"
#include "./include/Pins.hpp"
#include "include/controllers/FanController.hpp"

#include "./include/TimeUtils.hpp"

namespace {
    uint32_t lastCommandTime = 0;

    struct ErrorState {
        bool active = false;
        uint32_t timestamp = 0;
        char reason[12] = {0};
    };

    ErrorState currentError;
}

namespace SafetyManager {
    void init() {
        lastCommandTime = millis();
    }

    void update() {
        TempController::updateThermalProtection();

        if (hasCriticalCondition()) {
            emergencyStop("TEMP_OVERLIMIT");
        }

        if (isAnyEndstopTriggered()) {
            emergencyStop("ENDSTOP_HIT");
            EndstopController::handle(10, nullptr);
        }
    }

    void notifyCommandReceived() {
        lastCommandTime = millis();
    }

    void emergencyStop(const char *reason) {
        currentError.active = true;
        currentError.timestamp = millis();
        strncpy(currentError.reason, reason, sizeof(currentError.reason) - 1);
        Serial.print(F("!!! EMERGENCY: "));
        Serial.println(reason);
        digitalWrite(PIN_HEATER, LOW);
        MotionController::emergencyStop();
        FanController::setSpeed(0);
        StateMachine::setState(MachineState::Error);
    }

    bool isInErrorState() {
        return currentError.active;
    }

    const char *getErrorReason() {
        return currentError.active ? currentError.reason : "No Error";
    }

    uint32_t getErrorTimestamp() {
        return currentError.active ? TimeUtils::elapsed(currentError.timestamp) : 0;
    }

    bool clearError() {
        if (!currentError.active) return false;
        currentError.active = false;
        currentError.timestamp = 0;
        currentError.reason[0] = '\0';

        if (StateMachine::getState() == MachineState::Error) {
            StateMachine::setState(MachineState::Idle);
            Serial.println(F("ERROR CLEARED"));
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
