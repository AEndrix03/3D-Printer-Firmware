#ifdef PLATFORM_ESP32

#include "../../include/hal/esp32/ESP32HAL.hpp"
#include "../../include/hal/McuHAL.hpp"
#include <Arduino.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

namespace hal {

    namespace serial {
        void begin(uint32_t baud) { ::Serial.begin(baud); }

        uint8_t available() {
            const int n = ::Serial.available();
            return n > 255 ? 255 : (uint8_t) n;
        }

        int read() { return ::Serial.read(); }

        void write(uint8_t byte) { ::Serial.write(byte); }

        void write(const uint8_t *buffer, uint8_t size) { ::Serial.write(buffer, size); }

        void writeP(const char *flashText) { ::Serial.print(flashText); }

        void flush() { ::Serial.flush(); }

        // DA FARE su ESP32: riconoscere '!' alla ricezione (callback della UART)
        bool takeAbortRequest() { return false; }
    }

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

    static portMUX_TYPE halMux = portMUX_INITIALIZER_UNLOCKED;
    static hw_timer_t *stepHwTimer = nullptr;
    static void (*stepHandler)() = nullptr;

    static void IRAM_ATTR onStepTimer() {
        if (stepHandler) stepHandler();
    }

    uint8_t irqSave() {
        portENTER_CRITICAL(&halMux);
        return 0;
    }

    void irqRestore(uint8_t) { portEXIT_CRITICAL(&halMux); }

    namespace stepTimer {
        void start(void (*handler)()) {
            stepHandler = handler;
            stepHwTimer = timerBegin(0, 40, true); // 80 MHz / 40 = 2 MHz
            timerAttachInterrupt(stepHwTimer, &onStepTimer, true);
            timerAlarmWrite(stepHwTimer, 2000, true);
            timerAlarmEnable(stepHwTimer);
        }

        void setInterval(uint16_t ticks) {
            if (stepHwTimer) timerAlarmWrite(stepHwTimer, ticks, true);
        }

        uint16_t takeMaxHandlerTicks() { return 0; }

        uint16_t takeOverruns() { return 0; }
    }

    uint16_t minFreeRam() { return 0; }

    bool stackGuardIntact() { return true; }

    ResetCause getResetCause() {
        switch (esp_reset_reason()) {
            case ESP_RST_POWERON: return RESET_POWERON;
            case ESP_RST_EXT: return RESET_EXTERNAL;
            case ESP_RST_BROWNOUT: return RESET_BROWNOUT;
            case ESP_RST_SW: return RESET_SOFT;
            case ESP_RST_INT_WDT:
            case ESP_RST_TASK_WDT:
            case ESP_RST_WDT: return RESET_WATCHDOG;
            default: return RESET_UNKNOWN;
        }
    }

    const char *getPlatformName() { return "ESP32"; }

    uint32_t getFreeHeap() { return ESP.getFreeHeap(); }

    uint32_t getTotalHeap() { return ESP.getHeapSize(); }
}

#endif