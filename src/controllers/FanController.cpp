#include <Arduino.h>
#include "../include/controllers/FanController.hpp"
#include "../include/Pins.hpp"
#include "../include/ParamParser.hpp"
#include "../include/CompactResponse.hpp"

namespace FanController {
    void init() {
        pinMode(PIN_FAN, OUTPUT);
        analogWrite(PIN_FAN, 0);
    }

    void handle(uint8_t code, const char *params) {
        if (code != 10) {
            CompactResponse::reject(PSTR("UNK_FCODE"));
            return;
        }

        float value;
        if (!ParamParser::get(params, 'S', value)) {
            CompactResponse::reject(PSTR("NO_PARAM"));
            return;
        }
        if (value < 0.0f) value = 0.0f;
        if (value > 255.0f) value = 255.0f;

        setSpeed((uint8_t) value);
        CompactResponse::begin(PSTR("FAN"));
        CompactResponse::addInt((int32_t) value);
        CompactResponse::end();
    }

    void setSpeed(uint8_t pwm) {
        // NB: su Arduino Uno A2 non e' un pin PWM: analogWrite si comporta da on/off (soglia 128)
        analogWrite(PIN_FAN, pwm);
    }
}
