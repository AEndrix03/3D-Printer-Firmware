//
// Created by redeg on 24/06/2024.
//

#ifndef INC_3D_PRINTER_FIRMWARE_SERIALHANDLER_H
#define INC_3D_PRINTER_FIRMWARE_SERIALHANDLER_H

#include "serial/SerialListener.h"
#include "io/commands/Command.h"

class SerialCommandHandler : protected SerialListener<String> {
public:
    void doAction(String commandString);
};


#endif //INC_3D_PRINTER_FIRMWARE_SERIALHANDLER_H
