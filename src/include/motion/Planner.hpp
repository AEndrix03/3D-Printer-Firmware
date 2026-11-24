#pragma once

#include <stdint.h>

/**
 * @brief Coda dei movimenti con controllo di arresto, in soli interi.
 *
 * La pianificazione vera (geometria, limiti degli assi, velocita' alle giunzioni,
 * linear advance) la fa l'host, in doppia precisione, e arriva gia' fatta con ogni
 * movimento (struct Plan). Qui resta solo quello che deve valere SEMPRE, anche se
 * l'host sbaglia o smette di inviare:
 *
 *   la macchina deve potersi fermare entro i movimenti gia' in coda.
 *
 * Per questo ogni blocco entra al massimo alla velocita' chiesta dall'host, ma mai
 * piu' veloce di quanto consenta la frenata lungo i blocchi che seguono (l'ultimo
 * in coda finisce da fermo). Quando l'host tiene la coda piena il risultato coincide
 * con il suo piano; quando resta indietro la macchina rallenta da sola.
 *
 * Unita' (uguali per tutti i blocchi, cosi' le velocita' si confrontano alle giunzioni):
 *   velocita'      1/64 mm/s lungo il percorso
 *   accelerazione  mm/s^2 lungo il percorso
 *   scala          step dell'asse dominante per mm di percorso, in 1/16
 */
namespace Planner {
    enum : uint8_t { AX_X = 0, AX_Y, AX_Z, AX_E, AX_COUNT };

    // 16 blocchi (15 utilizzabili) da 48 byte
    constexpr uint8_t BLOCK_BUFFER_SIZE = 16;

    /** Step massimi per asse in un blocco: l'interrupt lavora a 16 bit. */
    constexpr int32_t MAX_BLOCK_STEPS = 32767;

    constexpr uint8_t FLAG_RECALCULATE = 0x01;  // profilo da ricalcolare
    constexpr uint8_t FLAG_BUSY = 0x04;         // in esecuzione: lo Stepper ne e' proprietario
    constexpr uint8_t FLAG_SEEK_ENDSTOP = 0x08; // fermarsi al finecorsa (homing / diagnosi)

    /** Il piano di un movimento, calcolato dall'host (o da moveAxis per i movimenti interni). */
    struct Plan {
        uint16_t nominalSpeed; // 1/64 mm/s
        uint16_t acceleration; // mm/s^2
        uint16_t stepScale;    // step dominanti per mm, in 1/16
        uint16_t entrySpeed;   // 1/64 mm/s: velocita' massima di ingresso (0 = da fermo)
    };

    struct Block {
        // --- usati dallo Stepper ---
        uint16_t steps[AX_COUNT];
        uint16_t stepEventCount;  // step dell'asse dominante
        uint16_t decelerateAfter; // step dopo il quale inizia la decelerazione
        uint32_t accelK;          // accelerazione in (step/s per tick di timer) * 65536
        uint32_t commandNumber;   // N del comando che ha generato il blocco
        uint16_t nominalRate;     // step/s
        uint16_t initialRate;
        uint16_t finalRate;
        uint8_t directionBits;    // bit a = 1 se l'asse a va in negativo
        volatile uint8_t flags;

        // --- usati dal controllo di arresto (velocita' al quadrato, unita' comuni) ---
        uint32_t entrySqr;        // velocita' di ingresso attuale
        uint32_t maxEntrySqr;     // quella chiesta dall'host
        uint32_t deltaSqr;        // quanto puo' cambiare lungo il blocco (2*a*d), saturato
        uint32_t accel2;          // 2 * accelerazione di pianificazione, in step/s^2
        uint16_t stepScale;
        uint16_t standstillRate;  // velocita' di partenza/arrivo "da fermo", step/s
    };

    void init();

    bool isFull();

    bool isEmpty();

    /** Blocchi in coda, compreso quello in esecuzione. */
    uint8_t queued();

    /**
     * @brief Accoda un movimento. Il chiamante deve aver verificato !isFull().
     * @param delta step con segno per X, Y, Z, E, ognuno entro MAX_BLOCK_STEPS
     * @return false se il movimento e' nullo (nessuno step)
     */
    bool addBlock(const int32_t *delta, const Plan &plan, uint32_t commandNumber, uint8_t flags = 0);

    /** Posizione (step) alla fine dell'ultimo blocco accodato. */
    int32_t position(uint8_t axis);

    void setPosition(uint8_t axis, int32_t steps);

    /** Svuota la coda. Da chiamare solo a Stepper fermo o da sezione critica. */
    void flush();

    // --- interfaccia per lo Stepper (chiamate dall'interrupt) ---
    Block *acquireBlock();

    void releaseBlock();
}
