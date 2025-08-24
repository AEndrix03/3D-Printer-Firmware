#include <Arduino.h>
#include "../include/controllers/FanController.hpp"
#include "../include/Pins.hpp"

namespace FanController {
    void init() {
        pinMode(PIN_FAN, OUTPUT);
        analogWrite(PIN_FAN, 0);
    }

    void handle(uint8_t code, const char *params) {
        // int -> uint8_t
        if (code == 10) {
            const char *p = strchr(params, 'S');
            if (p) {
                uint8_t value = (uint8_t) atoi(p + 1); // Cast esplicito
                setSpeed(value);
            }
        }
    }

    void setSpeed(uint8_t pwm) {
        analogWrite(PIN_FAN, pwm);
    }
}
