//
// Created by redeg on 27/04/2025.
//

#include "include/BusyHandler.hpp"

#include "./include/TimeUtils.hpp"

namespace {
    bool active = false;
    uint32_t lastBusyTime = 0;
    constexpr uint32_t BUSY_INTERVAL = 1000;
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
        if (active && TimeUtils::hasElapsed(lastBusyTime, BUSY_INTERVAL)) {
            Serial.println(F("BUSY"));
            lastBusyTime = millis();
        }
    }
}
