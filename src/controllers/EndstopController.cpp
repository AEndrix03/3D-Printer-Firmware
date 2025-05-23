#include <Arduino.h>
#include "../include/controllers/EndstopController.hpp"
#include "../include/Pins.hpp"
#include "../include/endstop/EndstopConfig.hpp"

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
        if (!EndstopConfig::ENDSTOP_ENABLED_X) return false;
        bool raw = digitalRead(PIN_ENDSTOP_X);
        return EndstopConfig::ENDSTOP_INVERTED_X ? (raw == HIGH) : (raw == LOW);
    }


    bool isTriggeredY() {
        if (!EndstopConfig::ENDSTOP_ENABLED_Y) return false;

        bool raw = digitalRead(PIN_ENDSTOP_Y);
        return EndstopConfig::ENDSTOP_INVERTED_Y ? (raw == HIGH) : (raw == LOW);
    }

    bool isTriggeredZ() {
        if (!EndstopConfig::ENDSTOP_ENABLED_Z) return false;
        bool raw = digitalRead(PIN_ENDSTOP_Z);
        return EndstopConfig::ENDSTOP_INVERTED_Z ? (raw == HIGH) : (raw == LOW);
    }

    bool isAnyTriggered() {
        return isTriggeredX() || isTriggeredY() || isTriggeredZ();
    }
}
