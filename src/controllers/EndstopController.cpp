#include <Arduino.h>
#include "../include/controllers/EndstopController.hpp"
#include "../include/Pins.hpp"

namespace EndstopController {

    void init() {
        pinMode(PIN_ENDSTOP_X, INPUT_PULLUP);
        pinMode(PIN_ENDSTOP_Y, INPUT_PULLUP);
        pinMode(PIN_ENDSTOP_Z, INPUT_PULLUP);
    }

    void handle(int code, const char *params) {
        if (code == 10) {
            Serial.print(F("ENDSTOPS: X="));
            Serial.print(isTriggeredX());
            Serial.print(F(" Y="));
            Serial.print(isTriggeredY());
            Serial.print(F(" Z="));
            Serial.println(isTriggeredZ());
        }
    }

    bool isTriggeredX() {
        return digitalRead(PIN_ENDSTOP_X) == LOW;
    }

    bool isTriggeredY() {
        return digitalRead(PIN_ENDSTOP_Y) == LOW;
    }

    bool isTriggeredZ() {
        return digitalRead(PIN_ENDSTOP_Z) == LOW;
    }
}
