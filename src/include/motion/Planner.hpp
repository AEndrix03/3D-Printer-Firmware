#pragma once

#include <stdint.h>

/**
 * @brief Coda dei movimenti con lookahead.
 *
 * Ogni comando di movimento diventa un blocco (segmento rettilineo su 4 assi).
 * Il planner decide a che velocita' attraversare ogni giunzione ("junction
 * deviation", come grbl) e ricalcola i profili trapezoidali dei blocchi in coda
 * cosi' che la macchina non debba fermarsi tra un segmento e il successivo.
 * Lo Stepper (interrupt) consuma i blocchi dalla coda.
 *
 * Tutti i calcoli in virgola mobile avvengono qui, nel loop principale:
 * l'interrupt usa solo interi.
 */
namespace Planner {
    enum : uint8_t { AX_X = 0, AX_Y, AX_Z, AX_E, AX_COUNT };

    // 8 blocchi (7 utilizzabili): compromesso tra lookahead e RAM dell'Uno (60 byte l'uno)
    constexpr uint8_t BLOCK_BUFFER_SIZE = 8;

    constexpr uint8_t FLAG_RECALCULATE = 0x01;    // profilo da ricalcolare
    constexpr uint8_t FLAG_NOMINAL_LENGTH = 0x02; // il blocco e' abbastanza lungo da raggiungere la velocita' nominale
    constexpr uint8_t FLAG_BUSY = 0x04;           // in esecuzione: lo Stepper ne e' proprietario
    constexpr uint8_t FLAG_SEEK_ENDSTOP = 0x08;   // fermarsi al finecorsa (homing / diagnosi)
    constexpr uint8_t FLAG_HAS_XYZ = 0x10;        // il blocco muove almeno un asse cartesiano

    struct Block {
        // --- usati dallo Stepper (interi) ---
        uint32_t steps[AX_COUNT];
        uint32_t stepEventCount;   // step dell'asse dominante
        uint32_t decelerateAfter;  // step dopo il quale inizia la decelerazione
        uint32_t accelK;           // accelerazione in (step/s per tick di timer) * 65536
        uint32_t commandNumber;    // N del comando che ha generato il blocco
        uint16_t nominalRate;      // step/s
        uint16_t initialRate;
        uint16_t finalRate;
        uint8_t directionBits;     // bit a = 1 se l'asse a va in negativo
        volatile uint8_t flags;

        // --- usati dal planner (float) ---
        float millimeters;
        float acceleration;        // mm/s^2 lungo il percorso
        float nominalSpeed;        // mm/s
        float entrySpeed;
        float maxEntrySpeed;
    };

    void init();

    bool isFull();

    bool isEmpty();

    /**
     * @brief Accoda un segmento. Il chiamante deve aver verificato !isFull().
     * @param delta step con segno per X, Y, Z, E
     * @param feedrate mm/min lungo il percorso XYZ (lungo E se sola estrusione)
     * @param flags FLAG_SEEK_ENDSTOP oppure 0
     * @return false se il segmento e' nullo (nessuno step)
     */
    bool addLine(const int32_t *delta, float feedrate, uint32_t commandNumber, uint8_t flags = 0);

    /** Posizione (step) alla fine dell'ultimo blocco accodato. */
    int32_t position(uint8_t axis);

    void setPosition(uint8_t axis, int32_t steps);

    /** Svuota la coda. Da chiamare solo a Stepper fermo o da sezione critica. */
    void flush();

    // --- interfaccia per lo Stepper (chiamate dall'interrupt) ---
    Block *acquireBlock();

    void releaseBlock();
}
