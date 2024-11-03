//
// Created by redeg on 03/11/2024.
//

#ifndef INC_3D_PRINTER_FIRMWARE_LED_H
#define INC_3D_PRINTER_FIRMWARE_LED_H


#include "io/Throwable.h"
#include "sensors/Sensor.h"

class Led : public Sensor {
public:
    explicit Led(SensorEnum s);
    void setup(SensorEnum ledType) override;
    bool doAction(char action) override;
    ~Led() override = default;
protected:
    void turnOn();
    void turnOff();
    void toggle();
private:
    bool on;
    int pin;
};


#endif //INC_3D_PRINTER_FIRMWARE_LED_H
