//
// Created by redeg on 24/06/2024.
//

#ifndef INC_3D_PRINTER_FIRMWARE_COMMANDFACTORY_H
#define INC_3D_PRINTER_FIRMWARE_COMMANDFACTORY_H

#include "Arduino.h"
#include "commands/Command.h"

class CommandFactory {
public:
    template<typename T>
    Command& createCommand(String command);
};


#endif //INC_3D_PRINTER_FIRMWARE_COMMANDFACTORY_H
