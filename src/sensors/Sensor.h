//
// Created by redeg on 24/06/2024.
//

#ifndef INC_3D_PRINTER_FIRMWARE_SENSOR_H
#define INC_3D_PRINTER_FIRMWARE_SENSOR_H

#include "SensorEnum.h"

class Sensor {
public:
    virtual void setup(SensorEnum type) = 0;
    virtual bool doAction(char action, double* returnValue) = 0;
    virtual ~Sensor() = default;
};

#endif //INC_3D_PRINTER_FIRMWARE_SENSOR_H
