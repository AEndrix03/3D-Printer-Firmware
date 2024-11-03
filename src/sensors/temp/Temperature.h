//
// Created by redeg on 03/11/2024.
//

#ifndef INC_3D_PRINTER_FIRMWARE_TEMPERATURE_H
#define INC_3D_PRINTER_FIRMWARE_TEMPERATURE_H


#include "sensors/Sensor.h"

class Temperature : public Sensor {
public:
    Temperature();
    void setup(SensorEnum ignored) override;
    bool doAction(char action, double* returnValue) override;
    ~Temperature() override = default;
protected:
    double read() const;
private:
    int pin;
    double lastTemp;
};


#endif //INC_3D_PRINTER_FIRMWARE_TEMPERATURE_H
