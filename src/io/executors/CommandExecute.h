//
// Created by redeg on 24/06/2024.
//

#ifndef INC_3D_PRINTER_FIRMWARE_COMMANDEXECUTE_H
#define INC_3D_PRINTER_FIRMWARE_COMMANDEXECUTE_H

#include "io/commands/Command.h"

class CommandExecute {
protected:
    virtual void execute(Command* arguments) = 0;
};

#endif //INC_3D_PRINTER_FIRMWARE_COMMANDEXECUTE_H
