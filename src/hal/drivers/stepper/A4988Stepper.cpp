#include <Arduino.h>
#include "./../../../include/hal/drivers/stepper/A4988Stepper.hpp"

A4988Stepper::A4988Stepper(uint8_t stepPin, uint8_t dirPin, uint8_t enPin)
        : _step(stepPin), _dir(dirPin), _en(enPin) {}

void A4988Stepper::init() {
    pinMode(_step, OUTPUT);
    pinMode(_dir, OUTPUT);
    if (_en != 255) {
        pinMode(_en, OUTPUT);
        digitalWrite(_en, LOW); // abilita motore
    }
}

void A4988Stepper::setDirection(bool dir) {
    digitalWrite(_dir, dir ? HIGH : LOW);
}

void A4988Stepper::step() {
    digitalWrite(_step, HIGH);
    delayMicroseconds(2); // minimo richiesto per A4988
    digitalWrite(_step, LOW);
    delayMicroseconds(2);
}

void A4988Stepper::enable(bool on) {
    if (_en != 255) {
        digitalWrite(_en, on ? LOW : HIGH); // LOW = enable
    }
}
