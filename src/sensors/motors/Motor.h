//
// Created by redeg on 12/08/2024.
//

#ifndef INC_3D_PRINTER_FIRMWARE_MOTOR_H
#define INC_3D_PRINTER_FIRMWARE_MOTOR_H


#include <Arduino.h>
#include "sensors/Sensor.h"
#include "io/Throwable.h"

class Motor : public Sensor, protected Throwable {
public:
    explicit Motor(SensorEnum motorType);
    void setup(SensorEnum motorType) override;
    bool doAction(char action) override;
    ~Motor() override = default;
protected:
    void handleException() override;
    void move(bool up);
private:
    int stepPin = -1;
    int dirPin = -1;
    int endStopPin = -1;
    int dirVoltage = HIGH;
    int speed = 0;
};


#endif //INC_3D_PRINTER_FIRMWARE_MOTOR_H
