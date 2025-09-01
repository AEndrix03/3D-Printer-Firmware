#ifdef PLATFORM_ARDUINO

#include "../../include/hal/arduino/ArduinoHAL.hpp"
#include "../../include/hal/McuHAL.hpp"
#include <Arduino.h>
#include <avr/wdt.h>

namespace hal {

    class ArduinoSerial : public hal::Serial {
    public:
        void begin(uint32_t baud) override {
            ::Serial.begin(baud);
            while (!::Serial);
        }

        void end() override { ::Serial.end(); }
        int available() override { return ::Serial.available(); }
        int read() override { return ::Serial.read(); }
        size_t write(uint8_t byte) override { return ::Serial.write(byte); }
        size_t write(const uint8_t *buffer, size_t size) override { return ::Serial.write(buffer, size); }
        size_t print(const char *str) override { return ::Serial.print(str); }
        size_t println(const char *str) override { return ::Serial.println(str); }
        size_t print(int value) override { return ::Serial.print(value); }
        size_t println(int value) override { return ::Serial.println(value); }
        size_t print(float value, int decimals) override { return ::Serial.print(value, decimals); }
        size_t println(float value, int decimals) override { return ::Serial.println(value, decimals); }
        size_t print(uint32_t value) override { return ::Serial.print(value); }
        size_t println(uint32_t value) override { return ::Serial.println(value); }
        size_t print(const __FlashStringHelper *str) override { return ::Serial.print(str); }
        size_t println(const __FlashStringHelper *str) override { return ::Serial.println(str); }
        void flush() override { ::Serial.flush(); }
        operator bool() override { return ::Serial; }
    };

    static ArduinoSerial arduinoSerial;
    Serial *serial = &arduinoSerial;

    void halPinMode(uint8_t pin, HalPinMode mode) {
        switch (mode) {
            case HAL_INPUT: ::pinMode(pin, INPUT); break;
            case HAL_OUTPUT: ::pinMode(pin, OUTPUT); break;
            case HAL_INPUT_PULLUP: ::pinMode(pin, INPUT_PULLUP); break;
            case HAL_INPUT_PULLDOWN: ::pinMode(pin, INPUT); break; // Arduino UNO doesn't support
        }
    }

    void writeDigital(uint8_t pin, bool value) { ::digitalWrite(pin, value ? HIGH : LOW); }
    bool readDigital(uint8_t pin) { return ::digitalRead(pin) == HIGH; }
    int readAnalog(uint8_t pin) { return ::analogRead(pin); }
    void writePwm(uint8_t pin, uint8_t value) { ::analogWrite(pin, value); }
    void setupPwm(uint8_t pin) { ::pinMode(pin, OUTPUT); }
    uint32_t halMillis() { return ::millis(); }
    uint32_t halMicros() { return ::micros(); }
    void halDelay(uint32_t ms) { ::delay(ms); }
    void delayMicroseconds(uint32_t us) { ::delayMicroseconds(us); }

    namespace watchdog {
        void enable(uint16_t timeout_ms) {
            uint8_t wdt_val;
            if (timeout_ms <= 15) wdt_val = WDTO_15MS;
            else if (timeout_ms <= 30) wdt_val = WDTO_30MS;
            else if (timeout_ms <= 60) wdt_val = WDTO_60MS;
            else if (timeout_ms <= 120) wdt_val = WDTO_120MS;
            else if (timeout_ms <= 250) wdt_val = WDTO_250MS;
            else if (timeout_ms <= 500) wdt_val = WDTO_500MS;
            else if (timeout_ms <= 1000) wdt_val = WDTO_1S;
            else if (timeout_ms <= 2000) wdt_val = WDTO_2S;
            else if (timeout_ms <= 4000) wdt_val = WDTO_4S;
            else wdt_val = WDTO_8S;

            wdt_enable(wdt_val);
        }

        void disable() { wdt_disable(); }
        void reset() { wdt_reset(); }
    }

    void init() {}
    void reset() { asm volatile ("jmp 0"); }
    const char *getPlatformName() { return "Arduino AVR"; }
    uint32_t getFreeHeap() {
        return -1; //TODO:
    }
    uint32_t getTotalHeap() { return 2048; }
}
#endif