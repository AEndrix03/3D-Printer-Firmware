#include <Arduino.h>
#include "../include/controllers/FanController.hpp"
#include "../include/Pins.hpp"

namespace FanController {

    void init() {
        pinMode(PIN_FAN, OUTPUT);
        analogWrite(PIN_FAN, 0);
    }

    void handle(int code, const char *params) {
        if (code == 10) {
            const char *p = strchr(params, 'S');
            if (p) {
                uint8_t value = atoi(p + 1);
                setSpeed(value);
            }
        }
    }

    void setSpeed(uint8_t pwm) {
        analogWrite(PIN_FAN, pwm);
    }
}
