#pragma once

#include <stdint.h>
#include "../motion/Position.hpp"
#include "../motion/Planner.hpp"

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
     *  - 12:  imposta la posizione corrente (X Y Z) - attende la fine dei movimenti.
     *         Gli assi impostati diventano riferiti: da li' valgono i limiti di corsa
     *  - 99:  diagnosi asse
     *  - 114: posizione reale in questo istante -> "CRT POS x y z <riferita 0/1>"
     *  - 115: step/mm, limiti della macchina e posizione in step -> "SPM ...", "LIM ...", "CRT STP x y z"
     * I movimenti arrivano solo in formato binario, gia' pianificati (handleStreamMove).
     */
    void handle(uint8_t code, const char *params, uint32_t commandNumber);

    /**
     * @brief Movimento ricevuto in formato binario (vedi MoveFrame.hpp): step relativi e
     * piano di velocita' gia' calcolati dall'host. Rifiuta (ERR LIMIT) i movimenti oltre il
     * volume di lavoro.
     * @param delta step con segno per X, Y, Z, E
     * @param plan nominalSpeed = 0: velocita' nominale e accelerazione invariate
     *
     * Dopo un rifiuto o un arresto i movimenti binari successivi vengono scartati
     * (ERR SKIPPED) finche' non arriva un comando di testo: l'host puo' averne altri
     * gia' in volo, e non devono partire da una posizione che non e' quella prevista.
     */
    void handleStreamMove(const int32_t *delta, Planner::Plan plan, uint32_t commandNumber);

    /** Un comando di testo e' arrivato: l'host si e' riallineato, i movimenti binari ripartono. */
    void releaseStream();

    /** Da chiamare spesso: esegue l'arresto se l'host ha inviato '!'. */
    void pollAbort();

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

    /**
     * Da' o toglie corrente ai motori (pin di enable comune). Togliendola la posizione
     * perde il riferimento: serve un nuovo homing (o M12) prima di fidarsene.
     */
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
