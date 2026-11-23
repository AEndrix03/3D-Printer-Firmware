#include "../include/hal/FastPin.hpp"
#include <Arduino.h>
#include "../include/controllers/EndstopController.hpp"
#include "../include/Pins.hpp"
#include "../include/endstop/EndstopConfig.hpp"
#include "../include/CompactResponse.hpp"

namespace EndstopController {
    void init() {
        pinMode(PIN_ENDSTOP_X, INPUT_PULLUP);
        pinMode(PIN_ENDSTOP_Y, INPUT_PULLUP);
        pinMode(PIN_ENDSTOP_Z, INPUT_PULLUP);
    }

    void handle(uint8_t code, const char *) {
        if (code != 10) {
            CompactResponse::reject(PSTR("UNK_ECODE"));
            return;
        }
        CompactResponse::begin(PSTR("END"), true);
        CompactResponse::addInt(isTriggeredX());
        CompactResponse::addInt(isTriggeredY());
        CompactResponse::addInt(isTriggeredZ());
        CompactResponse::end();
    }

    bool isTriggeredX() {
        if (!EndstopConfig::ENDSTOP_ENABLED_X) return false;
        bool raw = hal::fastRead(PIN_ENDSTOP_X);
        return EndstopConfig::ENDSTOP_INVERTED_X ? (raw == HIGH) : (raw == LOW);
    }

    bool isTriggeredY() {
        if (!EndstopConfig::ENDSTOP_ENABLED_Y) return false;
        bool raw = hal::fastRead(PIN_ENDSTOP_Y);
        return EndstopConfig::ENDSTOP_INVERTED_Y ? (raw == HIGH) : (raw == LOW);
    }

    bool isTriggeredZ() {
        if (!EndstopConfig::ENDSTOP_ENABLED_Z) return false;
        bool raw = hal::fastRead(PIN_ENDSTOP_Z);
        return EndstopConfig::ENDSTOP_INVERTED_Z ? (raw == HIGH) : (raw == LOW);
    }

    bool isAnyTriggered() {
        return isTriggeredX() || isTriggeredY() || isTriggeredZ();
    }
}