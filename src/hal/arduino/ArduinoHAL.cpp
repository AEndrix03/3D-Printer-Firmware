#include <Arduino.h>
#include "../../include/hal/arduino/ArduinoHAL.hpp"

namespace hal {

    void writeDigital(uint8_t pin, bool value) {
        digitalWrite(pin, value ? HIGH : LOW);
    }

    bool readDigital(uint8_t pin) {
        return digitalRead(pin) == HIGH;
    }

    int readAnalog(uint8_t pin) {
        return analogRead(pin);
    }

    void delayMicroseconds(uint32_t us) {
        ::delayMicroseconds(us);
    }

    void setupPwm(uint8_t pin) {
        pinMode(pin, OUTPUT);  // PWM è implicito
    }

    void writePwm(uint8_t pin, uint8_t value) {
        analogWrite(pin, value);  // range 0–255 su Arduino UNO
    }

    void configureInputPullup(uint8_t pin) {
        pinMode(pin, INPUT_PULLUP);
    }

}
