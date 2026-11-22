#pragma once

#include <stdint.h>

/**
 * @brief Generatore di step guidato da interrupt di timer.
 *
 * Esegue i blocchi del Planner: Bresenham su 4 assi e rampa di velocita' in
 * aritmetica intera. Girando in interrupt, il loop principale resta libero di
 * ricevere il comando successivo mentre la macchina si muove: e' questo che
 * permette al planner di unire i segmenti senza fermate.
 *
 * La legge di velocita' e' sempre limitata in accelerazione: se il blocco viene
 * caricato a una velocita' diversa da quella pianificata, la raggiunge in rampa
 * invece di saltarci.
 */
namespace Stepper {
    void init();

    /** true se non c'e' nessun blocco in esecuzione ne' in coda. */
    bool isIdle();

    /** Posizione reale in step (lettura atomica). */
    int32_t position(uint8_t axis);

    void setPosition(uint8_t axis, int32_t steps);

    /** N dell'ultimo comando di movimento completamente eseguito. */
    uint32_t lastCompletedCommand();

    /**
     * @brief Arresto controllato: decelera lungo il percorso e poi svuota la coda.
     * La posizione resta esatta (nessuno step perso).
     */
    void hold();

    /** true mentre un arresto controllato e' in corso. */
    bool isHolding();

    /** Arresto immediato (emergenze): ferma gli step e svuota la coda. */
    void quickStop();

    /**
     * @brief true (una sola volta) dopo che un arresto ha svuotato la coda:
     * il planner deve riallineare la propria posizione a quella reale.
     */
    bool consumeStopEvent();

    /** true se l'ultimo blocco FLAG_SEEK_ENDSTOP si e' fermato sul finecorsa. */
    bool endstopHit();

    /** Routine dell'interrupt (pubblica per HAL e simulatore). */
    void isr();
}
