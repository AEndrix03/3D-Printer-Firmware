//
// Created by redeg on 12/08/2024.
//

#ifndef INC_3D_PRINTER_FIRMWARE_SENSORMAP_H
#define INC_3D_PRINTER_FIRMWARE_SENSORMAP_H

#include "Sensor.h"
#include "SensorEnum.h"

class SensorMap {
public:
    static SensorEnum get(char type);
    static SensorType getType(char type);
};

SensorEnum SensorMap::get(const char type) {
    switch (type) {
        case 0:
            return MOTOR_X;
        case 1:
            return MOTOR_Y;
        case 2:
            return MOTOR_Z;
        case 3:
            return MOTOR_A;
        case 4:
            return TEMP;
        default:
            return ERROR_LED;
    }
}

SensorType SensorMap::getType(char type) {
    switch (type) {
        case 0:
        case 1:
        case 2:
        case 3:
            return MOTOR;
        case 4:
            return TEMPERATURE;
        case 5:
            return ENDSTOP;
        default:
            return LED;
    }
}

#endif //INC_3D_PRINTER_FIRMWARE_SENSORMAP_H
