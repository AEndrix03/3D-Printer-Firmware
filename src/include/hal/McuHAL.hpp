#pragma once

#include <stdint.h>
#include <stddef.h>

// Forward declare FlashStringHelper for F() macro support
class __FlashStringHelper;

namespace hal {

    // ===== GPIO =====
    enum HalPinMode {
        HAL_INPUT,
        HAL_OUTPUT,
        HAL_INPUT_PULLUP,
        HAL_INPUT_PULLDOWN
    };

    void halPinMode(uint8_t pin, HalPinMode mode);

    void writeDigital(uint8_t pin, bool value);

    bool readDigital(uint8_t pin);

    // ===== Analog =====
    int readAnalog(uint8_t pin);

    void writePwm(uint8_t pin, uint8_t value);

    void setupPwm(uint8_t pin);

    // ===== Time =====
    uint32_t halMillis();

    uint32_t halMicros();

    void halDelay(uint32_t ms);

    void delayMicroseconds(uint32_t us);

    // ===== Serial =====
    class Serial {
    public:
        virtual void begin(uint32_t baud) = 0;

        virtual void end() = 0;

        virtual int available() = 0;

        virtual int read() = 0;

        virtual size_t write(uint8_t byte) = 0;

        virtual size_t write(const uint8_t *buffer, size_t size) = 0;

        virtual size_t print(const char *str) = 0;

        virtual size_t println(const char *str) = 0;

        virtual size_t print(int value) = 0;

        virtual size_t println(int value) = 0;

        virtual size_t print(float value, int decimals = 2) = 0;

        virtual size_t println(float value, int decimals = 2) = 0;

        virtual size_t print(uint32_t value) = 0;

        virtual size_t println(uint32_t value) = 0;

        virtual void flush() = 0;

        virtual operator bool() = 0;

        // F() macro support
        virtual size_t print(const __FlashStringHelper *str) = 0;

        virtual size_t println(const __FlashStringHelper *str) = 0;
    };

    extern Serial *serial;

    // ===== Watchdog =====
    namespace watchdog {
        void enable(uint16_t timeout_ms);

        void disable();

        void reset();
    }

    // ===== System =====
    void init();

    void reset();

    // ===== Platform specific =====
    const char *getPlatformName();

    uint32_t getFreeHeap();

    uint32_t getTotalHeap();
}