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
    PGM_P stateName() {
        switch (StateMachine::getState()) {
            case MachineState::Idle:
                return PSTR("Idle");
            case MachineState::Homing:
                return PSTR("Homing");
            case MachineState::Printing:
                return PSTR("Printing");
            case MachineState::Paused:
                return PSTR("Paused");
            default:
                return PSTR("Error");
        }
    }

    PGM_P powerName() {
        switch (SafetyManager::getPowerState()) {
            case SafetyManager::ACTIVE:
                return PSTR("Active");
            case SafetyManager::IDLE:
                return PSTR("Idle");
            case SafetyManager::SLEEP:
                return PSTR("Sleep");
            default:
                return PSTR("Deep");
        }
    }
}

namespace SystemController {
    PGM_P resetCauseName() {
        switch (hal::getResetCause()) {
            case hal::RESET_POWERON:
                return PSTR("POWERON");
            case hal::RESET_EXTERNAL:
                return PSTR("EXTERNAL");
            case hal::RESET_BROWNOUT:
                return PSTR("BROWNOUT");
            case hal::RESET_WATCHDOG:
                return PSTR("WDT");
            case hal::RESET_SOFT:
                return PSTR("SOFT");
            default:
                return PSTR("UNKNOWN");
        }
    }

    void handle(uint8_t code, const char *params) {
        switch (code) {
            case 0:
                homing();
                break;
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

    void homing() {
        if (StateMachine::getState() != MachineState::Idle) {
            CompactResponse::reject(PSTR("NOT_IDLE"));
            return;
        }

        StateMachine::setState(MachineState::Homing);
        const bool ok = MotionController::homeAllAxes();

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
