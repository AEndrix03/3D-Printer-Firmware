#ifdef PLATFORM_ESP32

#include "../../include/hal/esp32/ESP32HAL.hpp"
#include "../../include/hal/McuHAL.hpp"
#include <Arduino.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

namespace hal {

    class ESP32Serial : public hal::Serial {
    public:
        void begin(uint32_t baud) override { ::Serial.begin(baud); }

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

    static ESP32Serial esp32Serial;
    Serial *serial = &esp32Serial;

    void halPinMode(uint8_t pin, HalPinMode mode) {
        switch (mode) {
            case HAL_INPUT:
                ::pinMode(pin, INPUT);
                break;
            case HAL_OUTPUT:
                ::pinMode(pin, OUTPUT);
                break;
            case HAL_INPUT_PULLUP:
                ::pinMode(pin, INPUT_PULLUP);
                break;
            case HAL_INPUT_PULLDOWN:
                ::pinMode(pin, INPUT_PULLDOWN);
                break;
        }
    }

    void writeDigital(uint8_t pin, bool value) { ::digitalWrite(pin, value ? HIGH : LOW); }

    bool readDigital(uint8_t pin) { return ::digitalRead(pin) == HIGH; }

    int readAnalog(uint8_t pin) { return ::analogRead(pin); }

    uint32_t halMillis() { return ::millis(); }

    uint32_t halMicros() { return ::micros(); }

    void halDelay(uint32_t ms) { ::delay(ms); }

    void delayMicroseconds(uint32_t us) { ::delayMicroseconds(us); }

    void writePwm(uint8_t pin, uint8_t value) {
        static uint8_t channels[40] = {0};
        static bool initialized[40] = {false};

        if (!initialized[pin]) {
            ledcSetup(channels[pin], 5000, 8);
            ledcAttachPin(pin, channels[pin]);
            initialized[pin] = true;
        }
        ledcWrite(channels[pin], value);
    }

    void setupPwm(uint8_t pin) { ::pinMode(pin, OUTPUT); }

    namespace watchdog {
        void enable(uint16_t timeout_ms) {
            esp_task_wdt_init(timeout_ms / 1000, true);
            esp_task_wdt_add(NULL);
        }

        void disable() {
            esp_task_wdt_delete(NULL);
            esp_task_wdt_deinit();
        }

        void reset() {
            esp_task_wdt_reset();
        }
    }

    void init() { setCpuFrequencyMhz(240); }

    void reset() { esp_restart(); }

    const char *getPlatformName() { return "ESP32"; }

    uint32_t getFreeHeap() { return ESP.getFreeHeap(); }

    uint32_t getTotalHeap() { return ESP.getHeapSize(); }
}

#endif