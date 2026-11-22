#ifdef PLATFORM_ARDUINO

#include "../../include/hal/arduino/ArduinoHAL.hpp"
#include "../../include/hal/McuHAL.hpp"
#include <Arduino.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include "../../include/Pins.hpp"

// ---------------------------------------------------------------------------
// Reset cause tracking.
// MCUSR va letto prima possibile; alcuni bootloader (Optiboot dell'Uno) lo
// azzerano, quindi il watchdog viene anche tracciato tramite un marker in
// .noinit scritto dall'interrupt del watchdog stesso (la RAM sopravvive al
// reset, non al power cycle).
// ---------------------------------------------------------------------------
namespace {
    constexpr uint16_t BOOT_MAGIC = 0x3D9A;

    struct BootInfo {
        uint16_t magic;
        uint8_t cause;
    };

    BootInfo bootInfo __attribute__((section(".noinit")));
    uint8_t mcusrMirror __attribute__((section(".noinit")));
    hal::ResetCause resetCause = hal::RESET_UNKNOWN;
}

extern int __heap_start;
extern int *__brkval;

void captureResetFlags() __attribute__((naked, used, section(".init3")));

void captureResetFlags() {
    mcusrMirror = MCUSR;
    MCUSR = 0;
    wdt_disable();
}

// Il watchdog lavora in modalita' interrupt+reset: prima di resettare spegne
// l'heater e registra la causa, poi forza il reset entro 15 ms.
ISR(WDT_vect) {
    digitalWrite(PIN_HEATER, LOW);
    bootInfo.magic = BOOT_MAGIC;
    bootInfo.cause = hal::RESET_WATCHDOG;
    wdt_enable(WDTO_15MS);
    for (;;) {}
}

namespace {
    void (*stepHandler)() = nullptr;
}

ISR(TIMER1_COMPA_vect) {
    if (stepHandler) stepHandler();
}

namespace hal {

    uint8_t irqSave() {
        const uint8_t state = SREG;
        cli();
        return state;
    }

    void irqRestore(uint8_t state) { SREG = state; }

    namespace stepTimer {
        void start(void (*handler)()) {
            const uint8_t state = irqSave();
            stepHandler = handler;
            TCCR1A = 0;
            TCCR1B = _BV(WGM12) | _BV(CS11); // CTC, prescaler 8 -> 2 MHz
            TCNT1 = 0;
            OCR1A = 2000;
            TIMSK1 |= _BV(OCIE1A);
            irqRestore(state);
        }

        void setInterval(uint16_t ticks) {
            // In CTC il contatore riparte da zero a ogni scadenza: se la routine ha gia'
            // superato il nuovo valore, la scadenza andrebbe persa per un giro intero (32 ms).
            const uint16_t now = TCNT1;
            if (ticks < now + 16) ticks = now + 16;
            OCR1A = ticks;
        }
    }

    class ArduinoSerial : public hal::Serial {
    public:
        void begin(uint32_t baud) override {
            ::Serial.begin(baud);
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
            WDTCSR |= _BV(WDIE); // interrupt + reset: vedi ISR(WDT_vect)
        }

        void disable() { wdt_disable(); }
        void reset() { wdt_reset(); }
    }

    void init() {
        const bool ramSurvived = (bootInfo.magic == BOOT_MAGIC);

        if (ramSurvived && (bootInfo.cause == RESET_WATCHDOG || bootInfo.cause == RESET_SOFT)) {
            resetCause = (ResetCause) bootInfo.cause;
        } else if (mcusrMirror & _BV(WDRF)) {
            resetCause = RESET_WATCHDOG;
        } else if (mcusrMirror & _BV(PORF)) {
            resetCause = RESET_POWERON;
        } else if (mcusrMirror & _BV(BORF)) {
            resetCause = RESET_BROWNOUT;
        } else if (mcusrMirror & _BV(EXTRF)) {
            resetCause = RESET_EXTERNAL;
        } else {
            // MCUSR azzerato dal bootloader: RAM intatta = reset a caldo (pin RESET / DTR)
            resetCause = ramSurvived ? RESET_EXTERNAL : RESET_POWERON;
        }

        bootInfo.magic = BOOT_MAGIC;
        bootInfo.cause = RESET_UNKNOWN;
    }

    void reset() {
        ::Serial.flush();
        cli();
        bootInfo.magic = BOOT_MAGIC;
        bootInfo.cause = RESET_SOFT;
        wdt_enable(WDTO_15MS); // senza WDIE: reset hardware pulito di tutte le periferiche
        for (;;) {}
    }

    ResetCause getResetCause() { return resetCause; }

    const char *getPlatformName() { return "Arduino AVR"; }

    uint32_t getFreeHeap() {
        int v;
        return (uint32_t) ((int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval));
    }
    uint32_t getTotalHeap() { return 2048; }
}
#endif