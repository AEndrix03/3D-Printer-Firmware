#pragma once

#include <stdint.h>
#include "../motion/Position.hpp"

/**
 * @brief Gestisce i movimenti della macchina lungo gli assi XYZ, coordinati con l'estrusore.
 *
 * I movimenti sono segmenti rettilinei (Bresenham su 4 assi) con rampa di
 * accelerazione/decelerazione; archi e spline vengono spezzati dal driver.
 * Sono bloccanti, ma durante l'esecuzione restano attivi watchdog, protezioni
 * termiche, messaggi BUSY e l'abort dall'host (carattere '!').
 */
namespace MotionController {
    /**
     * @brief Inizializza i driver stepper.
     */
    void init();

    /**
     * @brief Gestisce un comando di movimento.
     *  - 0:   emergency stop
     *  - 10:  movimento relativo  (X Y Z E in mm, F in mm/min)
     *  - 11:  movimento assoluto  (X Y Z assoluti, assi omessi invariati; E relativo)
     *  - 12:  imposta la posizione corrente (X Y Z)
     *  - 99:  diagnosi asse
     *  - 114: posizione corrente -> "CRT POS x y z"
     */
    void handle(uint8_t code, const char *params);

    /**
     * @brief Estrusione senza movimento XYZ (usata da ExtruderController).
     * @return false se interrotta
     */
    bool moveExtruder(int32_t steps, float feedrate);

    /** Abilita/disabilita i driver (se il pin ENABLE e' configurato). */
    void setEnabled(bool enabled);

    /**
     * @brief Segnala l'arresto di emergenza all'host e disabilita i driver.
     */
    void emergencyStop();

    /**
     * @brief Homing di tutti gli assi. Gli assi senza finecorsa abilitato vengono azzerati sul posto.
     * @return false se un finecorsa non scatta entro la corsa massima o se interrotto
     */
    bool homeAllAxes();

    position::Position getPosition();
}
