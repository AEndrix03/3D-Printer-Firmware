#include <Arduino.h>
#include "./include/SafetyManager.hpp"
#include "./include/controllers/TempController.hpp"
#include "./include/controllers/MotionController.hpp"
#include "./include/controllers/EndstopController.hpp"
#include "./include/StateMachine.hpp"
#include "./include/Pins.hpp"

namespace {
    unsigned long lastCommandTime = 0;
    // Rimosso TIMEOUT_MS - no più timeout rigido

    // Power management states (commenti per implementazione futura)
    // enum PowerState { ACTIVE, IDLE, SLEEP };
    // PowerState currentPowerState = ACTIVE;
    // constexpr unsigned long IDLE_THRESHOLD = 300000; // 5 min per entrare in idle
    // constexpr unsigned long SLEEP_THRESHOLD = 1800000; // 30 min per sleep
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
        Serial.print(F("!!! EMERGENCY: "));
        Serial.println(reason);
        MotionController::emergencyStop();
        digitalWrite(PIN_HEATER, LOW);
        StateMachine::setState(MachineState::Error);
    }

    // TODO: Implementare funzioni power management
    /*
    void enterIdleMode() {
        // Riduce frequenza PWM ventole
        // Riduce luminosità LED se presenti
        // Rallenta polling sensori non critici
        currentPowerState = IDLE;
    }

    void enterSleepMode() {
        // Disabilita stepper (se safe)
        // Mette ADC in power-down tra letture
        // Riduce frequenza CPU se possibile
        // Mantiene solo serial e safety attivi
        currentPowerState = SLEEP;
    }

    void wakeUp() {
        // Riattiva tutti i sistemi
        // Ripristina frequenza normale
        currentPowerState = ACTIVE;
    }
    */
}
