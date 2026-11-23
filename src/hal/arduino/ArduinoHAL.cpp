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
    constexpr uint8_t STACK_CANARY = 0xC5;

    void (*stepHandler)() = nullptr;
    volatile uint16_t maxHandlerTicks = 0;
    volatile uint16_t handlerOverruns = 0;
}

namespace {
    constexpr uint8_t RX_SIZE = 64; // potenza di 2
    constexpr uint8_t RX_MASK = RX_SIZE - 1;

    volatile uint8_t rxBuffer[RX_SIZE];
    volatile uint8_t rxHead = 0; // scritto solo dall'interrupt
    volatile uint8_t rxTail = 0; // scritto solo dal ciclo principale
    bool txUsed = false;
    volatile bool abortFlag = false;
}

ISR(USART_RX_vect) {
    const uint8_t byte = UDR0;
    if (byte == '!') {
        abortFlag = true; // comando in tempo reale: non entra nell'anello
        return;
    }
    const uint8_t next = (uint8_t) (rxHead + 1) & RX_MASK;
    // Anello pieno: il byte viene scartato, se ne accorge il checksum del comando
    if (next != rxTail) {
        rxBuffer[rxHead] = byte;
        rxHead = next;
    }
}

ISR(TIMER1_COMPA_vect) {
    if (stepHandler) stepHandler();

    // In CTC il contatore riparte da zero alla scadenza: qui vale il tempo speso nell'handler
    const uint16_t elapsed = TCNT1;
    if (elapsed > maxHandlerTicks) maxHandlerTicks = elapsed;
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
            if (ticks < now + 16) {
                ticks = now + 16;
                if (handlerOverruns < 65535) handlerOverruns++;
            }
            OCR1A = ticks;
        }

        uint16_t takeMaxHandlerTicks() {
            const uint8_t state = irqSave();
            const uint16_t value = maxHandlerTicks;
            maxHandlerTicks = 0;
            irqRestore(state);
            return value;
        }

        uint16_t takeOverruns() {
            const uint8_t state = irqSave();
            const uint16_t value = handlerOverruns;
            handlerOverruns = 0;
            irqRestore(state);
            return value;
        }
    }

    uint16_t minFreeRam() {
        // La RAM libera e' stata "dipinta" all'avvio: i byte ancora intatti a partire
        // dalla fine dei dati sono quelli che lo stack non ha mai raggiunto.
        const uint8_t *p = (const uint8_t *) &__heap_start;
        uint16_t count = 0;
        while (p < (const uint8_t *) RAMEND && *p == STACK_CANARY) {
            ++p;
            ++count;
        }
        return count;
    }

    bool stackGuardIntact() {
        // Lo stack puo' scavalcare qualche byte senza scriverlo (variabili locali non
        // inizializzate): si controlla una fascia intera, non un byte solo.
        const uint8_t *p = (const uint8_t *) &__heap_start + STACK_GUARD_BYTES - 8;
        for (uint8_t i = 0; i < 8; ++i) {
            if (p[i] != STACK_CANARY) return false;
        }
        return true;
    }

    // -----------------------------------------------------------------------
    // Seriale: driver USART minimo al posto di HardwareSerial di Arduino.
    //  - ricezione in interrupt su un anello di RX_SIZE byte;
    //  - trasmissione bloccante, senza buffer: le risposte vengono gia' composte
    //    per intero in CompactResponse, un secondo buffer sarebbe RAM sprecata.
    //    Gli interrupt restano attivi, quindi step e ricezione non si fermano.
    // -----------------------------------------------------------------------
    namespace serial {
        void begin(uint32_t baud) {
            const uint8_t state = irqSave();
            UCSR0A = _BV(U2X0); // 8 campioni per bit: errore di baud minore a 16 MHz
            UBRR0 = (uint16_t) ((F_CPU / 4 / baud - 1) / 2);
            UCSR0C = _BV(UCSZ01) | _BV(UCSZ00); // 8N1
            UCSR0B = _BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0);
            rxHead = rxTail = 0;
            irqRestore(state);
        }

        uint8_t available() {
            return (uint8_t) (rxHead - rxTail) & RX_MASK;
        }

        int read() {
            if (rxHead == rxTail) return -1;
            const uint8_t value = rxBuffer[rxTail];
            rxTail = (uint8_t) (rxTail + 1) & RX_MASK;
            return value;
        }

        void write(uint8_t byte) {
            while (!(UCSR0A & _BV(UDRE0))) {}
            txUsed = true;
            UCSR0A |= _BV(TXC0); // azzera il flag di fine trasmissione (si azzera scrivendo 1)
            UDR0 = byte;
        }

        void write(const uint8_t *buffer, uint8_t size) {
            while (size--) write(*buffer++);
        }

        void writeP(const char *flashText) {
            uint8_t c;
            while ((c = pgm_read_byte(flashText++)) != 0) write(c);
        }

        bool takeAbortRequest() {
            if (!abortFlag) return false;
            abortFlag = false;
            return true;
        }

        void flush() {
            if (!(UCSR0B & _BV(TXEN0))) return;
            while (!(UCSR0A & _BV(UDRE0))) {}
            // TXC e' valido solo dopo almeno una scrittura: UDRE vuoto + TXC = filo libero
            while (!(UCSR0A & _BV(TXC0)) && txUsed) {}
        }
    }

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

        // Dipinge la RAM tra la fine dei dati e lo stack attuale (vedi minFreeRam)
        uint8_t marker;
        for (uint8_t *q = (uint8_t *) &__heap_start; q < &marker - 16; ++q) *q = STACK_CANARY;
    }

    void reset() {
        serial::flush();
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