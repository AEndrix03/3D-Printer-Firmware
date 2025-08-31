//
// Created by redeg on 30/08/2025.
//
// ESP32HAL.cpp
#include "../../include/hal/esp32/ESP32HAL.hpp"
#include <Arduino.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

namespace hal {

    // Serial implementation for ESP32
    class ESP32Serial : public Serial {
    public:
        void begin(uint32_t baud) override {
            ::Serial.begin(baud);
            // ESP32 doesn't need to wait for USB
        }

        void end() override {
            ::Serial.end();
        }

        int available() override {
            return ::Serial.available();
        }

        int read() override {
            return ::Serial.read();
        }

        size_t write(uint8_t byte) override {
            return ::Serial.write(byte);
        }

        size_t write(const uint8_t *buffer, size_t size) override {
            return ::Serial.write(buffer, size);
        }

        size_t print(const char *str) override {
            return ::Serial.print(str);
        }

        size_t println(const char *str) override {
            return ::Serial.println(str);
        }

        size_t print(int value) override {
            return ::Serial.print(value);
        }

        size_t println(int value) override {
            return ::Serial.println(value);
        }

        size_t print(float value, int decimals) override {
            return ::Serial.print(value, decimals);
        }

        size_t println(float value, int decimals) override {
            return ::Serial.println(value, decimals);
        }

        size_t print(uint32_t value) override {
            return ::Serial.print(value);
        }

        size_t println(uint32_t value) override {
            return ::Serial.println(value);
        }

        void flush() override {
            ::Serial.flush();
        }

        operator bool() override {
            return ::Serial;
        }
    };

    static ESP32Serial esp32Serial;
    Serial *serial = &esp32Serial;

    // ===== GPIO =====
    void pinMode(uint8_t pin, PinMode mode) {
        switch (mode) {
            case INPUT:
                ::pinMode(pin, INPUT);
                break;
            case OUTPUT:
                ::pinMode(pin, OUTPUT);
                break;
            case INPUT_PULLUP:
                ::pinMode(pin, INPUT_PULLUP);
                break;
            case INPUT_PULLDOWN:
                ::pinMode(pin, INPUT_PULLDOWN);
                break;
        }
    }

    void writeDigital(uint8_t pin, bool value) {
        ::digitalWrite(pin, value ? HIGH : LOW);
    }

    bool readDigital(uint8_t pin) {
        return ::digitalRead(pin) == HIGH;
    }

    // ===== Analog =====
    int readAnalog(uint8_t pin) {
        return ::analogRead(pin);
    }

    void writePwm(uint8_t pin, uint8_t value) {
        // ESP32 uses ledcWrite for PWM
        static uint8_t channels[40] = {0};
        static bool initialized[40] = {false};

        if (!initialized[pin]) {
            ledcSetup(channels[pin], 5000, 8); // 5kHz, 8-bit resolution
            ledcAttachPin(pin, channels[pin]);
            initialized[pin] = true;
        }

        ledcWrite(channels[pin], value);
    }

    void setupPwm(uint8_t pin) {
        ::pinMode(pin, OUTPUT);
    }

    // ===== Time =====
    uint32_t millis() {
        return ::millis();
    }

    uint32_t micros() {
        return ::micros();
    }

    void delay(uint32_t ms) {
        ::delay(ms);
    }

    void delayMicroseconds(uint32_t us) {
        ::delayMicroseconds(us);
    }

    // ===== Watchdog =====
    namespace watchdog {
        static esp_task_wdt_config_t wdt_config = {
                .timeout_ms = 2000,
                .idle_core_mask = 0,
                .trigger_panic = true
        };

        void enable(uint16_t timeout_ms) {
            wdt_config.timeout_ms = timeout_ms;
            esp_task_wdt_init(&wdt_config);
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

    // ===== System =====
    void init() {
        // ESP32 specific initialization
        setCpuFrequencyMhz(240);
    }

    void reset() {
        esp_restart();
    }

    // ===== Platform specific =====
    const char *getPlatformName() {
        return "ESP32";
    }

    uint32_t getFreeHeap() {
        return ESP.getFreeHeap();
    }

    uint32_t getTotalHeap() {
        return ESP.getHeapSize();
    }
}
