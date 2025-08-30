#include "./include/WatchdogHandler.hpp"
#include "./include/hal/McuHAL.hpp"

namespace WatchdogHandler {

    void enable() {
        hal::watchdog::enable(2000); // 2 seconds timeout
    }

    void disable() {
        hal::watchdog::disable();
    }

    void reset() {
        hal::watchdog::reset();
    }

}