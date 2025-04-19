#include <avr/wdt.h>
#include "./include/WatchdogHandler.hpp"

namespace WatchdogHandler {

    void enable() {
        wdt_enable(WDTO_2S); // 2 secondi
    }

    void disable() {
        wdt_disable();
    }

    void reset() {
        wdt_reset();
    }

}
