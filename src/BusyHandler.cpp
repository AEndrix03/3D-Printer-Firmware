//
// Created by redeg on 27/04/2025.
//

#include "include/BusyHandler.hpp"

namespace {
    bool active = false;
    unsigned long lastBusyTime = 0;
}

namespace BusyHandler {

    void start() {
        active = true;
        lastBusyTime = millis();
    }

    void stop() {
        active = false;
    }

    void update() {
        if (!active) return;

        if (millis() - lastBusyTime >= 1000) {
            Serial.println(F("BUSY"));
            lastBusyTime = millis();
        }
    }

}