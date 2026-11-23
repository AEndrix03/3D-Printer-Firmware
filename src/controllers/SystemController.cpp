#include <Arduino.h>
#include "../include/controllers/SystemController.hpp"
#include "../include/controllers/MotionController.hpp"
#include "../include/StateMachine.hpp"
#include "../include/SafetyManager.hpp"
#include "../include/SerialCommandReceiver.hpp"
#include "../include/ParamParser.hpp"
#include "../include/hal/McuHAL.hpp"
#include "../include/CompactResponse.hpp"

namespace {
    // Catene di if al posto degli switch: per uno switch che restituisce puntatori il
    // compilatore genera una tabella, e sull'AVR le tabelle finiscono in RAM.
    PGM_P stateName() {
        const MachineState state = StateMachine::getState();
        if (state == MachineState::Idle) return PSTR("Idle");
        if (state == MachineState::Homing) return PSTR("Homing");
        if (state == MachineState::Printing) return PSTR("Printing");
        if (state == MachineState::Paused) return PSTR("Paused");
        return PSTR("Error");
    }

    PGM_P powerName() {
        const SafetyManager::PowerState state = SafetyManager::getPowerState();
        if (state == SafetyManager::ACTIVE) return PSTR("Active");
        if (state == SafetyManager::IDLE) return PSTR("Idle");
        if (state == SafetyManager::SLEEP) return PSTR("Sleep");
        return PSTR("Deep");
    }
}

namespace SystemController {
    PGM_P resetCauseName() {
        const hal::ResetCause cause = hal::getResetCause();
        if (cause == hal::RESET_POWERON) return PSTR("POWERON");
        if (cause == hal::RESET_EXTERNAL) return PSTR("EXTERNAL");
        if (cause == hal::RESET_BROWNOUT) return PSTR("BROWNOUT");
        if (cause == hal::RESET_WATCHDOG) return PSTR("WDT");
        if (cause == hal::RESET_SOFT) return PSTR("SOFT");
        return PSTR("UNKNOWN");
    }

    void handle(uint8_t code, const char *params) {
        switch (code) {
            case 0: {
                // S0 [X] [Y] [Z]: assi da azzerare (nessuno = tutti)
                uint8_t axes = 0;
                if (strchr(params, 'X')) axes |= 0x01;
                if (strchr(params, 'Y')) axes |= 0x02;
                if (strchr(params, 'Z')) axes |= 0x04;
                homing(axes);
                break;
            }
            case 1:
                startPrint();
                break;
            case 2:
                pause();
                break;
            case 3:
                resume();
                break;
            case 4:
                emergencyReset();
                break;
            case 5:
                reset();
                break;
            case 6:
                clearError();
                break;
            case 7:
                printErrorStatus();
                break;
            case 8:
                printPowerStatus();
                break;
            case 9: {
                float state;
                if (ParamParser::get(params, 'P', state) && state >= 0.0f && state <= 3.0f) {
                    SafetyManager::forcePowerState((SafetyManager::PowerState) (uint8_t) state);
                } else {
                    CompactResponse::reject(PSTR("BAD_PARAM"));
                }
                break;
            }
            case 10:
                printStatus();
                break;
            case 11:
                printDiagnostics();
                break;
            case 12:
                endPrint();
                break;
            default:
                CompactResponse::reject(PSTR("UNK_SCODE"));
                break;
        }
    }

    void printStatus() {
        CompactResponse::sendData(PSTR("STA"), stateName());
    }

    void printDiagnostics() {
        // DIA <uptime s> <reset cause> <ultimo N> <err checksum> <err overflow> <ACK persi> <RAM libera>
        CompactResponse::begin(PSTR("DIA"));
        CompactResponse::addInt((int32_t) (hal::halMillis() / 1000UL));
        CompactResponse::addP(resetCauseName());
        CompactResponse::addInt((int32_t) SerialCommandReceiver::lastCommandNumber);
        CompactResponse::addInt(SerialCommandReceiver::checksumErrors);
        CompactResponse::addInt(SerialCommandReceiver::overflowErrors);
        CompactResponse::addInt(CompactResponse::getAckFailures());
        CompactResponse::addInt((int32_t) hal::getFreeHeap());
        CompactResponse::end();

        // PRF <RAM minima mai rimasta libera> <durata max interrupt step, us> <scadenze step mancate>
        // Gli ultimi due valori si riferiscono al periodo dall'ultima lettura.
        CompactResponse::begin(PSTR("PRF"));
        CompactResponse::addInt(hal::minFreeRam());
        CompactResponse::addInt(hal::stepTimer::takeMaxHandlerTicks() / 2);
        CompactResponse::addInt(hal::stepTimer::takeOverruns());
        CompactResponse::end();
    }

    void reset() {
        CompactResponse::sendData(PSTR("RST"), PSTR("BRUTAL"));
        hal::reset();
    }

    void emergencyReset() {
        CompactResponse::sendData(PSTR("RST"), PSTR("EMERGENCY"));
        hal::reset();
    }

    void startPrint() {
        if (StateMachine::getState() == MachineState::Idle) {
            StateMachine::setState(MachineState::Printing);
            CompactResponse::sendData(PSTR("STA"), PSTR("PRINTING"));
        } else {
            CompactResponse::reject(PSTR("NOT_IDLE"));
        }
    }

    void pause() {
        if (StateMachine::getState() == MachineState::Printing) {
            // i movimenti gia' accodati vengono completati: all'OK la macchina e' ferma
            if (!MotionController::synchronize()) return;
            StateMachine::setState(MachineState::Paused);
            CompactResponse::sendData(PSTR("STA"), PSTR("PAUSED"));
        } else {
            CompactResponse::reject(PSTR("NOT_PRINTING"));
        }
    }

    void resume() {
        if (StateMachine::getState() == MachineState::Paused) {
            StateMachine::setState(MachineState::Printing);
            CompactResponse::sendData(PSTR("STA"), PSTR("RESUMED"));
        } else {
            CompactResponse::reject(PSTR("NOT_PAUSED"));
        }
    }

    void endPrint() {
        if (!MotionController::synchronize()) return;
        const MachineState state = StateMachine::getState();
        if (state == MachineState::Printing || state == MachineState::Paused) {
            StateMachine::setState(MachineState::Idle);
        }
        // Idle: idempotente. Error: serve prima S6.
        if (StateMachine::getState() == MachineState::Idle) {
            CompactResponse::sendData(PSTR("STA"), PSTR("Idle"));
        } else {
            CompactResponse::reject(PSTR("INV_STATE"));
        }
    }

    void clearError() {
        CompactResponse::setResult(SafetyManager::clearError()
                                   ? CompactResponse::CANCELLED_ERROR
                                   : CompactResponse::NO_ERROR);
    }

    void printErrorStatus() {
        CompactResponse::begin(PSTR("ERR"), true);
        if (SafetyManager::isInErrorState()) {
            CompactResponse::addStr(SafetyManager::getErrorReason());
            CompactResponse::addInt((int32_t) (SafetyManager::getErrorTimestamp() / 1000UL));
        } else {
            CompactResponse::addP(PSTR("NONE"));
        }
        CompactResponse::end();
    }

    void homing(uint8_t axes) {
        if (StateMachine::getState() != MachineState::Idle) {
            CompactResponse::reject(PSTR("NOT_IDLE"));
            return;
        }

        StateMachine::setState(MachineState::Homing);
        const bool ok = MotionController::home(axes);

        // Una emergenza durante la ricerca dello zero lascia la macchina in Error
        if (StateMachine::getState() == MachineState::Homing) {
            StateMachine::setState(MachineState::Idle);
        }
        if (ok) CompactResponse::sendData(PSTR("HOM"), PSTR("DONE"));
    }

    void printPowerStatus() {
        CompactResponse::begin(PSTR("PWR"));
        CompactResponse::addP(powerName());
        CompactResponse::addInt((int32_t) SafetyManager::getInactiveTime());
        CompactResponse::end();
    }
}
