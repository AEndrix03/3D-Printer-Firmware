//
// Created by redeg on 13/08/2024.
//

#ifndef INC_3D_PRINTER_FIRMWARE_COMMANDEXECUTE_H
#define INC_3D_PRINTER_FIRMWARE_COMMANDEXECUTE_H

#include <WString.h>

class CommandExecute {
protected:
    virtual String execute() = 0;
    virtual ~CommandExecute() = default;
};

#endif //INC_3D_PRINTER_FIRMWARE_COMMANDEXECUTE_H
