//
// Created by redeg on 24/06/2024.
//

#ifndef INC_3D_PRINTER_FIRMWARE_LEDCOMMAND_H
#define INC_3D_PRINTER_FIRMWARE_LEDCOMMAND_H

#include "io/commands/Command.h"
#include "sensors/Sensor.h"

class LedCommand : protected Command {
public:
    LedCommand(String& identifier, CommandArgument* arguments, Sensor* sensors) : Command(identifier, arguments), sensors(sensors) {}
    ~LedCommand() override;
    bool subscribe() override;
    bool unsubscribe() override;
    void notifyAll() override;
private:
    Sensor* sensors;
};


#endif //INC_3D_PRINTER_FIRMWARE_LEDCOMMAND_H
