//
// Created by redeg on 27/04/2025.
//

#include "include/BusyHandler.hpp"
#include "./include/hal/McuHAL.hpp"
#include "./include/TimeUtils.hpp"

namespace {
    bool active = false;
    uint32_t lastBusyTime = 0;
    constexpr uint32_t BUSY_INTERVAL = 1000;
}

namespace BusyHandler {
    void start() {
        active = true;
        lastBusyTime = hal::halMillis();
    }

    void stop() {
        active = false;
    }

    void update() {
        if (active && TimeUtils::hasElapsed(lastBusyTime, BUSY_INTERVAL)) {
            hal::serial->println(F("BUSY"));
            lastBusyTime = hal::halMillis();
        }
    }
}