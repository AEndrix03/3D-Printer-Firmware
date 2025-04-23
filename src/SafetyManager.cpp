#include <Arduino.h>
#include "./include/SafetyManager.hpp"
#include "./include/controllers/TempController.hpp"
#include "./include/controllers/MotionController.hpp"
#include "./include/controllers/EndstopController.hpp"
#include "./include/StateMachine.hpp"
#include "./include/Pins.hpp"

namespace {
    unsigned long lastCommandTime = 0;
    const unsigned long TIMEOUT_MS = 600000;
}

namespace SafetyManager {
    void init() {
        lastCommandTime = millis();
    }

    void update() {
        if (hasTimedOut()) {
            emergencyStop("COMMAND TIMEOUT");
        }

        if (hasCriticalCondition()) {
            emergencyStop("TEMP OVERLIMIT");
        }

        if (isAnyEndstopTriggered()) {
            emergencyStop("ENDSTOP TRIGGERED DURING PRINTING PHASE");
            EndstopController::handle(10, nullptr);
        }
    }


    void notifyCommandReceived() {
        lastCommandTime = millis();
    }

    bool hasTimedOut() {
        return (millis() - lastCommandTime > TIMEOUT_MS);
    }

    bool hasCriticalCondition() {
        float temp = TempController::getTemperature();
        return temp > 250.0f;
    }

    bool isAnyEndstopTriggered() {
        return StateMachine::getState() == MachineState::Printing && EndstopController::isAnyTriggered();
    }

    void emergencyStop(const char *reason) {
        Serial.print(F("!!! EMERGENCY: "));
        Serial.println(reason);
        MotionController::emergencyStop();
        digitalWrite(PIN_HEATER, LOW); // ora è visibile
        StateMachine::setState(MachineState::Error);
    }
}
