//
// Created by redeg on 24/06/2024.
//

#ifndef INC_3D_PRINTER_FIRMWARE_COMMANDEXECUTOR_H
#define INC_3D_PRINTER_FIRMWARE_COMMANDEXECUTOR_H

#include "io/executors/CommandExecute.h"
#include "io/commands/Command.h"

class CommandExecutor {
protected:
    virtual bool subscribe(CommandExecute* listener) = 0;
    virtual bool unsubscribe(CommandExecute* listener) = 0;
    virtual void notifyAll() = 0;
};

#endif //INC_3D_PRINTER_FIRMWARE_COMMANDEXECUTOR_H
