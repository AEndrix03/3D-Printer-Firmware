#include <Arduino.h>
#include "./include/SafetyManager.hpp"
#include "./include/controllers/TempController.hpp"
#include "./include/controllers/MotionController.hpp"
#include "./include/controllers/EndstopController.hpp"
#include "./include/StateMachine.hpp"
#include "./include/Pins.hpp"
#include "include/controllers/FanController.hpp"

namespace {
    unsigned long lastCommandTime = 0;

    struct ErrorState {
        bool active = false;
        unsigned long timestamp = 0;
        char reason[32] = {0};
    };

    ErrorState currentError;
}

namespace SafetyManager {
    void init() {
        lastCommandTime = millis();
    }

    void update() {
        // Rimosse verifiche timeout - no più emergency stop per inattività

        if (hasCriticalCondition()) {
            emergencyStop("TEMP OVERLIMIT");
        }

        if (isAnyEndstopTriggered()) {
            emergencyStop("ENDSTOP TRIGGERED DURING PRINTING PHASE");
            EndstopController::handle(10, nullptr);
        }

        // TODO: Implementare power management
        // unsigned long inactiveTime = millis() - lastCommandTime;
        // if (inactiveTime > SLEEP_THRESHOLD) {
        //     enterSleepMode();
        // } else if (inactiveTime > IDLE_THRESHOLD) {
        //     enterIdleMode();
        // }
    }

    void notifyCommandReceived() {
        lastCommandTime = millis();
        // TODO: Wake up da sleep/idle mode se necessario
        // if (currentPowerState != ACTIVE) {
        //     wakeUp();
        // }
    }

    // Rimossa hasTimedOut() - non più necessaria

    bool hasCriticalCondition() {
        float temp = TempController::getTemperature();
        return temp > 250.0f;
    }

    bool isAnyEndstopTriggered() {
        return StateMachine::getState() == MachineState::Printing && EndstopController::isAnyTriggered();
    }

    void emergencyStop(const char *reason) {
        // Salva dettagli errore
        currentError.active = true;
        currentError.timestamp = millis();
        strncpy(currentError.reason, reason, sizeof(currentError.reason) - 1);
        Serial.print(F("!!! EMERGENCY: "));
        Serial.println(reason);
        // Spegni tutto in sicurezza
        digitalWrite(PIN_HEATER, LOW);
        MotionController::emergencyStop();
        FanController::setSpeed(0);
        // Vai in error state
        StateMachine::setState(MachineState::Error);
    }

    bool isInErrorState() {
        return currentError.active && StateMachine::getState() == MachineState::Error;
    }

    const char *getErrorReason() {
        return currentError.active ? currentError.reason : "No Error";
    }

    unsigned long getErrorTimestamp() {
        return currentError.timestamp;
    }

    bool clearError() {
        if (!currentError.active) return false;
        // Reset stato errore
        currentError.active = false;
        currentError.timestamp = 0;
        memset(currentError.reason, 0, sizeof(currentError.reason));

        // Torna in Idle solo se attualmente in Error
        if (StateMachine::getState() == MachineState::Error) {
            StateMachine::setState(MachineState::Idle);
            Serial.println(F("ERROR CLEARED - STATE -> IDLE"));
            return true;
        }

        return false;
    }
}
