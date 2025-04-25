#pragma once

#include <stdint.h>

namespace hal {
    void writeDigital(uint8_t pin, bool value);

    bool readDigital(uint8_t pin);

    int readAnalog(uint8_t pin);

    void delayMicroseconds(uint32_t us);

    void setupPwm(uint8_t pin);

    void writePwm(uint8_t pin, uint8_t value);

    void configureInputPullup(uint8_t pin);
}
