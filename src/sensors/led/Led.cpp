//
// Created by redeg on 03/11/2024.
//

#include <Arduino.h>
#include "Led.h"
#include "../SensorPinOut.h"

Led::Led(SensorEnum s) {
    setup(s);
}

void Led::setup(SensorEnum ledType) {
    if (ledType == ERROR_LED) {
        pin = pins::ERROR_LED;
    }

    pinMode(pin, OUTPUT);
}

bool Led::doAction(char action, double* returnValue) {
    if (action == 0) {
        turnOff();
    } else if (action == 1) {
        turnOn();
    } else {
        toggle();
    }

    return true;
}

void Led::turnOn() {
    on = true;
    digitalWrite(pin, HIGH);
}

void Led::turnOff() {
    on = false;
    digitalWrite(pin, LOW);
}

void Led::toggle() {
    if (on) {
        turnOff();
    } else {
        turnOn();
    }
}
