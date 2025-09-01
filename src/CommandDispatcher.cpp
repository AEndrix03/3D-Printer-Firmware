#include <Arduino.h>
#include "./include/CommandDispatcher.hpp"
#include "./include/controllers/MotionController.hpp"
#include "./include/controllers/TempController.hpp"
#include "./include/controllers/FanController.hpp"
#include "./include/controllers/SystemController.hpp"
#include "./include/controllers/EndstopController.hpp"
#include "./include/controllers/ExtruderController.hpp"
#include "./include/CompactResponse.hpp"

void CommandDispatcher::dispatch(const ParsedCommand &cmd) {
    switch (cmd.category) {
        case 'M':
            MotionController::handle(cmd.code, cmd.params);
            break;
        case 'A':
            ExtruderController::handle(cmd.code, cmd.params);
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
            CompactResponse::send(CompactResponse::INVALID_CATEGORY_ERROR, cmd.number);
            break;
    }
}
