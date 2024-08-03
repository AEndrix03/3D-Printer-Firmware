//
// Created by redeg on 24/06/2024.
//

#include "CommandListener.h"

CommandListener *CommandListener::getInstance() {
    if (instance == nullptr) {
        instance = new CommandListener();
    }

    if (instance->commandExecutors == nullptr) {
        instance->commandExecutors = new CommandExecutor[1];
    }
}
