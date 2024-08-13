//
// Created by redeg on 13/08/2024.
//

#ifndef INC_3D_PRINTER_FIRMWARE_COMMAND_H
#define INC_3D_PRINTER_FIRMWARE_COMMAND_H

#include <Arduino.h>
#include "CommandExecute.h"
#include "sensors/Sensor.h"

class Command : protected CommandExecute {
public:
    explicit Command(const String command);
    char getExecMode() const;
    char getId() const;
    char getAction() const;
    int getRepeat() const;
    String execute() override;
    virtual ~Command() = default;
protected:
    bool tcpExecute(Sensor* sensor) const;
    int udpExecute(Sensor* sensor) const;
private:
    char execMode;
    char id;
    char action;
    int repeat;
};


#endif //INC_3D_PRINTER_FIRMWARE_COMMAND_H
