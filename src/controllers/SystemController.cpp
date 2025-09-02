#include <Arduino.h>
#include "../include/controllers/SystemController.hpp"
#include "../include/controllers/MotionController.hpp"
#include "../include/StateMachine.hpp"
#include "../include/SafetyManager.hpp"
#include "../include/hal/McuHAL.hpp"
#include "../include/CompactResponse.hpp"

namespace SystemController {
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
                const char *p = strchr(params, 'P');
                if (p) {
                    int state = atoi(p + 1);
                    if (state >= 0 && state <= 3) {
                        SafetyManager::forcePowerState((SafetyManager::PowerState) state);
                    }
                }
                break;
            }
            case 10:
                printStatus();
                break;
            default:
                CompactResponse::sendData("ERR", "UNK_SCODE");
                break;
        }
    }

    void printStatus() {
        CompactResponse::sendData("STA", StateMachine::getStateName());
    }

    void reset() {
        CompactResponse::sendData("RST", "BRUTAL");
        hal::reset();
    }

    void startPrint() {
        if (StateMachine::getState() == MachineState::Idle) {
            StateMachine::setState(MachineState::Printing);
            CompactResponse::sendData("STA", "PRINTING");
        } else {
            CompactResponse::sendData("ERR", "NOT_IDLE");
        }
    }

    void pause() {
        if (StateMachine::getState() == MachineState::Printing) {
            StateMachine::setState(MachineState::Paused);
            CompactResponse::sendData("STA", "PAUSED");
        } else {
            CompactResponse::sendData("ERR", "NOT_PRINTING");
        }
    }

    void resume() {
        if (StateMachine::getState() == MachineState::Paused) {
            StateMachine::setState(MachineState::Printing);
            CompactResponse::sendData("STA", "RESUMED");
        } else {
            CompactResponse::sendData("ERR", "NOT_PAUSED");
        }
    }

    void emergencyReset() {
        CompactResponse::sendData("RST", "EMERGENCY");
        hal::watchdog::enable(15);
        while (1);
    }

    void clearError() {
        if (SafetyManager::clearError()) {
            CompactResponse::sendNoCommand(CompactResponse::CANCELLED_ERROR);
        } else {
            CompactResponse::sendNoCommand(CompactResponse::NO_ERROR);
        }
    }

    void printErrorStatus() {
        if (SafetyManager::isInErrorState()) {
            char errorData[16];
            snprintf(errorData, sizeof(errorData), "%s %u",
                     SafetyManager::getErrorReason(),
                     SafetyManager::getErrorTimestamp());
            CompactResponse::sendCriticalData("ERR", errorData);
        } else {
            CompactResponse::sendCriticalData("ERR", "NONE");
        }
    }

    void homing() {
        if (StateMachine::getState() == MachineState::Idle) {
            CompactResponse::sendData("HOM", "START");
            StateMachine::setState(MachineState::Homing);
            MotionController::homeAllAxes();
            StateMachine::setState(MachineState::Idle);
            CompactResponse::sendData("HOM", "DONE");
        } else {
            CompactResponse::sendData("ERR", "NOT_IDLE");
        }
    }

    void printPowerStatus() {
        char powerData[16];
        snprintf(powerData, sizeof(powerData), "%s %u",
                 SafetyManager::getPowerStateName(),
                 SafetyManager::getInactiveTime());
        CompactResponse::sendData("PWR", powerData);
    }
}