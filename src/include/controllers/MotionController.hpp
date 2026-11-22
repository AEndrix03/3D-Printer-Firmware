#pragma once

#include <stdint.h>
#include "../motion/Position.hpp"

/**
 * @brief Gestisce i movimenti della macchina lungo gli assi XYZ, coordinati con l'estrusore.
 *
 * I comandi di movimento vengono ACCODATI nel Planner e la risposta OK arriva
 * appena il segmento e' in coda (o appena si libera un posto): mentre la
 * macchina si muove il firmware riceve gia' i segmenti successivi, e il planner
 * li raccorda senza fermate. Gli step sono generati in interrupt (Stepper).
 * Archi e spline vengono spezzati in segmenti dal driver.
 */
namespace MotionController {
    void init();

    /** Da chiamare a ogni giro del loop principale. */
    void update();

    /**
     * @brief Gestisce un comando di movimento.
     *  - 0:   emergency stop
     *  - 10:  movimento relativo  (X Y Z E in mm, F in mm/min) - accodato
     *  - 11:  movimento assoluto  (X Y Z assoluti, assi omessi invariati; E relativo) - accodato
     *  - 12:  imposta la posizione corrente (X Y Z) - attende la fine dei movimenti
     *  - 99:  diagnosi asse
     *  - 114: posizione reale in questo istante -> "CRT POS x y z <riferita 0/1>"
     */
    void handle(uint8_t code, const char *params, uint32_t commandNumber);

    /**
     * @brief Accoda una estrusione senza movimento XYZ (usata da ExtruderController).
     * @return false se rifiutata o interrotta
     */
    bool queueExtruder(int32_t steps, float feedrate, uint32_t commandNumber);

    /**
     * @brief Attende che tutti i movimenti in coda siano terminati.
     * @return false se nel frattempo c'e' stato un abort o un errore
     */
    bool synchronize();

    /** true se non ci sono movimenti in corso ne' in coda. */
    bool isIdle();

    /**
     * @brief Ultimo comando la cui esecuzione e' completa: `current` se la macchina
     * e' ferma, altrimenti l'ultimo movimento terminato.
     */
    uint32_t lastCompletedCommand(uint32_t current);

    /** Arresto controllato richiesto dall'host ('!'): decelera e svuota la coda. */
    void abort();

    /** Abilita/disabilita i driver (se il pin ENABLE e' configurato). */
    void setEnabled(bool enabled);

    /**
     * @brief Arresto immediato di tutti i movimenti (emergenze).
     */
    void emergencyStop();

    /**
     * @brief Homing. Gli assi senza finecorsa abilitato vengono azzerati sul posto.
     * @param axes maschera: bit0 X, bit1 Y, bit2 Z (0 = tutti)
     * @return false se un finecorsa non scatta entro la corsa massima o se interrotto
     */
    bool home(uint8_t axes);

    /** true se la posizione ha un riferimento valido (homing o M12 dall'accensione). */
    bool isPositionReferenced();

    /** Posizione E reale in step. */
    int32_t extruderSteps();

    void resetExtruderSteps();

    position::Position getPosition();
}
