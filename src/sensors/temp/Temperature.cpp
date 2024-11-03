//
// Created by redeg on 03/11/2024.
//

#include <Arduino.h>
#include "Temperature.h"
#include "../SensorPinOut.h"
#include "../SensorsConfiguration.h"

Temperature::Temperature() {
    setup(TEMP);
}

void Temperature::setup(SensorEnum ignored) {
    pin = pins::TEMP;
    pinMode(pin, INPUT_PULLUP);
}

bool Temperature::doAction(char action, double* returnValue) {
    double value;
    if (action == 0) {
        value = lastTemp;
    } else {
        value = read();
        lastTemp = value;
    }
    returnValue = &value;
    return true;
}

double Temperature::read() const {
    return (1.0 / (1.0 / config::TEMP_SENSOR_0_TEMP_0 + (1.0 / config::TEMP_SENSOR_0_BETA) * log(((1023.0 / analogRead(pin) - 1.0) * config::TEMP_SENSOR_0_SER_RES) / config::TEMP_SENSOR_0_RES_0))) - 273.15;
}
