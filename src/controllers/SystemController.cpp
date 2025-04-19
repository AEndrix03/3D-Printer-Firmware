#include <Arduino.h>
#include "../include/controllers/SystemController.hpp"
#include "../include/StateMachine.hpp"

namespace SystemController {

    void handle(int code, const char *params) {
        switch (code) {
            case 10:
                printStatus();
                break;
            case 0:
                reset();
                break;
        }
    }

    void printStatus() {
        Serial.print(F("STATE: "));
        Serial.println(StateMachine::getStateName());
    }

    void reset() {
        asm volatile ("jmp 0");
    }
}
