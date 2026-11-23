#pragma once

#include <stdint.h>
#include <stddef.h>

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
    // Funzioni libere, risolte in compilazione: ogni piattaforma fornisce la propria
    // implementazione (nessuna tabella virtuale, nessuna chiamata indiretta).
    namespace serial {
        void begin(uint32_t baud);

        /// Byte ricevuti e non ancora letti.
        uint8_t available();

        /// Prossimo byte ricevuto, -1 se non ce ne sono.
        int read();

        void write(uint8_t byte);

        void write(const uint8_t *buffer, uint8_t size);

        /// Scrive una stringa che vive in flash (PSTR).
        void writeP(const char *flashText);

        /// Ritorna quando l'ultimo byte scritto e' uscito dal filo.
        void flush();

        /**
         * true (una sola volta) se dall'ultima chiamata e' arrivato il carattere di arresto
         * '!'. Viene riconosciuto alla ricezione, senza passare dal buffer: resta
         * immediato anche con altri comandi in attesa davanti a lui.
         */
        bool takeAbortRequest();
    }

    // ===== Interrupt =====
    /// Disabilita gli interrupt e restituisce lo stato precedente (sezioni critiche annidabili).
    uint8_t irqSave();

    void irqRestore(uint8_t state);

    // ===== Timer degli step =====
    namespace stepTimer {
        /// Risoluzione del timer: 2 MHz = 0,5 us per tick (Timer1 con prescaler 8 su AVR a 16 MHz)
        constexpr uint32_t TICKS_PER_SECOND = 2000000UL;

        /// Avvia il timer: `handler` viene chiamato in interrupt a ogni scadenza.
        void start(void (*handler)());

        /// Tick fino alla prossima scadenza. Chiamabile dall'handler.
        void setInterval(uint16_t ticks);

        /// Durata massima dell'handler (tick) dall'ultima lettura; la lettura la azzera.
        uint16_t takeMaxHandlerTicks();

        /// Quante volte l'handler ha sforato il proprio intervallo (CPU satura) dall'ultima lettura.
        uint16_t takeOverruns();
    }

    /// Minimo di RAM rimasta libera tra dati e stack dall'avvio (picco di stack reale).
    uint16_t minFreeRam();

    /// Zona di RAM, subito sopra i dati, che lo stack non deve mai raggiungere.
    constexpr uint16_t STACK_GUARD_BYTES = 128;

    /**
     * false se lo stack e' arrivato dentro la zona di guardia: restano meno di
     * STACK_GUARD_BYTES prima di sovrascrivere le variabili. Costo costante.
     */
    bool stackGuardIntact();

    // ===== Watchdog =====
    namespace watchdog {
        void enable(uint16_t timeout_ms);

        void disable();

        void reset();
    }

    // ===== System =====
    void init();

    /// Riavvia il microcontrollore in modo pulito (causa registrata come SOFT).
    void reset();

    // ===== Reset cause =====
    enum ResetCause : uint8_t {
        RESET_UNKNOWN = 0,
        RESET_POWERON,
        RESET_EXTERNAL,
        RESET_BROWNOUT,
        RESET_WATCHDOG,
        RESET_SOFT
    };

    /// Causa dell'ultimo reset, determinata all'avvio (valida dopo hal::init()).
    ResetCause getResetCause();

    // ===== Platform specific =====
    const char *getPlatformName();

    uint32_t getFreeHeap();

    uint32_t getTotalHeap();
}