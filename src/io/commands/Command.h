//
// Created by redeg on 24/06/2024.
//

#ifndef INC_3D_PRINTER_FIRMWARE_COMMAND_H
#define INC_3D_PRINTER_FIRMWARE_COMMAND_H

#include "Arduino.h"
#include "CommandArgument.h"
#include "io/executors/CommandExecutor.h"
#include "utils/std.h"

class Command : protected CommandExecutor {
public:
    Command(String& identifier, CommandArgument* arguments) : identifier(identifier), arguments(arguments) {}
    virtual ~Command() = 0;

    bool subscribe(CommandExecute *listener) override {
        listeners.push_back(listener);
    }

    bool unsubscribe(CommandExecute *listener) override {
        listeners.remove(listener);
    }

    void notifyAll() override {
        for (auto it: listeners) {}
    }

private:
    String identifier;
    CommandArgument* arguments;
    static std::Vector<CommandExecute*> listeners;
};


#endif //INC_3D_PRINTER_FIRMWARE_COMMAND_H
