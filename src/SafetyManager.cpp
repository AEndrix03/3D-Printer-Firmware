#include <Arduino.h>
#include "./include/SafetyManager.hpp"
#include "./include/controllers/TempController.hpp"
#include "./include/controllers/MotionController.hpp"
#include "./include/StateMachine.hpp"
#include "./include/Pins.hpp" // <--- Aggiunto qui

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

    void emergencyStop(const char *reason) {
        Serial.print(F("!!! EMERGENCY: "));
        Serial.println(reason);
        MotionController::emergencyStop();
        digitalWrite(PIN_HEATER, LOW); // ora è visibile
        StateMachine::setState(MachineState::Error);
    }

}
