#include <Arduino.h>
#include <avr/wdt.h>
#include "../include/controllers/SystemController.hpp"
#include "../include/controllers/MotionController.hpp"
#include "../include/StateMachine.hpp"

namespace SystemController {

    void handle(int code, const char *params) {
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
            case 10:
                printStatus();
                break;
        }
    }

    void printStatus() {
        Serial.print(F("STATE: "));
        Serial.println(StateMachine::getStateName());
    }

    void reset() {
        Serial.println("BRUTAL RESET TRIGGERED");
        asm volatile ("jmp 0");
    }

    void startPrint() {
        if (StateMachine::getState() == MachineState::Idle) {
            StateMachine::setState(MachineState::Printing);
            Serial.println(F("STATE -> PRINTING"));
        }
    }

    void pause() {
        if (StateMachine::getState() == MachineState::Printing) {
            StateMachine::setState(MachineState::Paused);
            Serial.println(F("STATE -> PAUSED"));
        }
    }

    void resume() {
        if (StateMachine::getState() == MachineState::Paused) {
            StateMachine::setState(MachineState::Printing);
            Serial.println(F("STATE -> RESUME PRINTING"));
        }
    }

    void emergencyReset() {
        Serial.println("FORCING RESET: EMERGENCY RESET TRIGGERED");

        wdt_enable(WDTO_15MS);
        while (1);
    }

    void homing() {
        if (StateMachine::getState() == MachineState::Idle) {
            Serial.println(F("STARTING HOMING..."));
            StateMachine::setState(MachineState::Homing);

            MotionController::homeAllAxes();

            StateMachine::setState(MachineState::Idle);
            Serial.println(F("HOMING COMPLETE. STATE -> IDLE"));
        }
    }


}
