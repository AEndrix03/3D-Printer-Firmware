#include "./include/SafetyManager.hpp"
#include "./include/hal/McuHAL.hpp"
#include "./include/controllers/TempController.hpp"
#include "./include/controllers/MotionController.hpp"
#include "./include/controllers/EndstopController.hpp"
#include "./include/controllers/FanController.hpp"
#include "./include/StateMachine.hpp"
#include "./include/Config.hpp"
#include "./include/CompactResponse.hpp"

// Tutti i tempi sono uint32_t in millisecondi e confrontati per differenza:
// nessun overflow a 18 ore, corretto anche al rollover di millis() (~49,7 giorni).
#define IDLE_TIMEOUT_MS 300000UL
#define SLEEP_TIMEOUT_MS 1800000UL
#define DEEP_SLEEP_TIMEOUT_MS 7200000UL

namespace {
    uint32_t lastCommandTime = 0;

    bool errorActive = false;
    uint32_t errorTime = 0;
    char errorReason[12] = {0};

    SafetyManager::PowerState powerState = SafetyManager::ACTIVE;
}

namespace SafetyManager {
    void init() {
        lastCommandTime = hal::halMillis();
        powerState = ACTIVE;
    }

    void update() {
        updatePowerManagement();
        TempController::update();

        // Host sparito con l'heater acceso: non si lascia scaldare all'infinito
        if (TempController::getTargetTemperature() > 0 &&
            hal::halMillis() - lastCommandTime >= HOST_LOST_TIMEOUT_MS) {
            TempController::setTargetTemperature(0);
            CompactResponse::sendData(PSTR("ALM"), PSTR("HOST_LOST"));
        }

        if (isAnyEndstopTriggered()) {
            emergencyStop(PSTR("ENDSTOP"));
            EndstopController::handle(10, nullptr);
        }
    }

    void notifyCommandReceived() {
        lastCommandTime = hal::halMillis();
        if (powerState != ACTIVE) {
            wakeUp();
        }
    }

    void updatePowerManagement() {
        const MachineState state = StateMachine::getState();
        if (state == MachineState::Printing || state == MachineState::Homing ||
            state == MachineState::Error) {
            return;
        }

        const uint32_t inactive = hal::halMillis() - lastCommandTime;

        PowerState target = ACTIVE;
        if (inactive >= DEEP_SLEEP_TIMEOUT_MS) {
            target = DEEP_SLEEP;
        } else if (inactive >= SLEEP_TIMEOUT_MS) {
            target = SLEEP;
        } else if (inactive >= IDLE_TIMEOUT_MS) {
            target = IDLE;
        }

        // Solo verso stati piu' profondi: il ritorno ad ACTIVE avviene esclusivamente
        // con wakeUp(). Cosi' il rollover di millis() non puo' risvegliare la macchina.
        if (target > powerState) {
            enterPowerState(target);
        }
    }

    void enterPowerState(PowerState newState) {
        switch (newState) {
            case IDLE:
                CompactResponse::sendData(PSTR("PWR"), PSTR("IDLE"));
                FanController::setSpeed(32);
                break;
            case SLEEP:
                CompactResponse::sendData(PSTR("PWR"), PSTR("SLEEP"));
                MotionController::setEnabled(false);
                TempController::setTargetTemperature(0);
                FanController::setSpeed(16);
                break;
            case DEEP_SLEEP:
                CompactResponse::sendData(PSTR("PWR"), PSTR("DEEP"));
                MotionController::setEnabled(false);
                TempController::setTargetTemperature(0);
                FanController::setSpeed(0);
                break;
            case ACTIVE:
                break;
        }
        powerState = newState;
    }

    void wakeUp() {
        if (powerState == ACTIVE) return;
        if (powerState >= SLEEP) {
            MotionController::setEnabled(true);
        }
        powerState = ACTIVE;
        CompactResponse::sendData(PSTR("PWR"), PSTR("ACTIVE"));
    }

    PowerState getPowerState() {
        return powerState;
    }

    uint32_t getInactiveTime() {
        return (hal::halMillis() - lastCommandTime) / 1000UL;
    }

    void forcePowerState(PowerState state) {
        if (state == ACTIVE) {
            wakeUp();
        } else {
            enterPowerState(state);
        }
    }

    void emergencyStop(PGM_P reason) {
        // L'heater viene spento sempre, anche se l'errore era gia' attivo
        TempController::forceOff();

        if (errorActive) return;

        errorActive = true;
        errorTime = hal::halMillis();
        strncpy_P(errorReason, reason, sizeof(errorReason) - 1);
        errorReason[sizeof(errorReason) - 1] = '\0';

        StateMachine::setState(MachineState::Error);
        MotionController::setEnabled(false);
        // La ventola resta com'e': spegnerla durante un allarme termico peggiorerebbe le cose.

        CompactResponse::sendData(PSTR("ALM"), reason);
    }

    bool isInErrorState() {
        return errorActive;
    }

    const char *getErrorReason() {
        return errorActive ? errorReason : "";
    }

    uint32_t getErrorTimestamp() {
        return errorActive ? (hal::halMillis() - errorTime) : 0;
    }

    bool clearError() {
        if (!errorActive) return false;

        errorActive = false;
        errorReason[0] = '\0';

        if (StateMachine::getState() == MachineState::Error) {
            StateMachine::setState(MachineState::Idle);
            MotionController::setEnabled(true);
            return true;
        }
        return false;
    }

    bool isAnyEndstopTriggered() {
        return StateMachine::getState() == MachineState::Printing &&
               EndstopController::isAnyTriggered();
    }
}
