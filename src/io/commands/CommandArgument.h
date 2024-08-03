//
// Created by redeg on 24/06/2024.
//

#ifndef INC_3D_PRINTER_FIRMWARE_COMMANDARGUMENT_H
#define INC_3D_PRINTER_FIRMWARE_COMMANDARGUMENT_H

#include "Arduino.h"

class CommandArgument {
public:
    explicit CommandArgument(String key, String value = "") : key(key), value(value) {}
    String getKey();
    String getValue();
private:
    String key;
    String value;
};

#endif //INC_3D_PRINTER_FIRMWARE_COMMANDARGUMENT_H
