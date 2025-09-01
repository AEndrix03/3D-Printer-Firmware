#include <Arduino.h>
#include "../include/controllers/FanController.hpp"
#include "../include/Pins.hpp"
#include "../include/CompactResponse.hpp"

namespace FanController {
    void init() {
        pinMode(PIN_FAN, OUTPUT);
        analogWrite(PIN_FAN, 0);
    }

    void handle(uint8_t code, const char *params) {
        if (code == 10) {
            const char *p = strchr(params, 'S');
            if (p) {
                uint8_t value = (uint8_t) atoi(p + 1);
                setSpeed(value);

                char fanData[8];
                snprintf(fanData, sizeof(fanData), "%u", value);
                CompactResponse::sendData("FAN", fanData);
            }
        }
    }

    void setSpeed(uint8_t pwm) {
        analogWrite(PIN_FAN, pwm);
    }
}