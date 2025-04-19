#include <Arduino.h>
#include "./include/CommandDispatcher.hpp"
#include "./include/controllers/MotionController.hpp"
#include "./include/controllers/TempController.hpp"
#include "./include/controllers/FanController.hpp"
#include "./include/controllers/SystemController.hpp"
#include "./include/controllers/EndstopController.hpp"

void CommandDispatcher::dispatch(const ParsedCommand &cmd) {
    switch (cmd.category) {
        case 'M':
            MotionController::handle(cmd.code, cmd.params);
            break;
        case 'T':
            TempController::handle(cmd.code, cmd.params);
            break;
        case 'F':
            FanController::handle(cmd.code, cmd.params);
            break;
        case 'S':
            SystemController::handle(cmd.code, cmd.params);
            break;
        case 'E':
            EndstopController::handle(cmd.code, cmd.params);
            break;
        default:
            Serial.print(F("ERR N"));
            Serial.print(cmd.number);
            Serial.println(F(" UNKNOWN_CATEGORY"));
            break;
    }
}
