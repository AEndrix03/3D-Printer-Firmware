#pragma once

#include "../hal/drivers/stepper/A4988Stepper.hpp"
#include "../motion/Position.hpp"

/**
 * @brief Gestisce i movimenti della macchina lungo gli assi XYZ.
 */
namespace MotionController {
    /**
     * @brief Inizializza eventuali driver drivers e limiti.
     */
    void init();

    /**
     * @brief Gestisce un comando di movimento.
     * @param code Codice specifico del comando (es. 10 = MOVE, 0 = STOP)
     * @param params Parametri associati (es. "X10.0 Y20.0 Z30.0 F1500")
     */
    void handle(uint8_t code, const char *params);

    /**
     * @brief Muove la testina/asse alla posizione specificata.
     * @param x Coordinata X (mm)
     * @param y Coordinata Y (mm)
     * @param z Coordinata Z (mm)
     * @param f Velocità di movimento (feedrate, mm/min)
     */
    void moveTo(float x, float y, float z, float f);

    void goTo(float targetX, float targetY, float targetZ, float feedrate);

    /**
     * @brief Arresta immediatamente tutti i movimenti.
     */
    void emergencyStop();

    /**
     * @brief Avvia il processo di homing per un certo asse
     * @param stepper Stepper dell'asse
     * @param isTriggered Trigger del finecorsa
     * @param stepsPerMm Steps per millimetro
     * @param feedrate Velocità di movimento (mm/min)
     * @param minRate Minima velocità di movimento (mm/min)
     * @param maxRate Massima velocità di movimento (mm/min)
     */
    void homeAxis(A4988Stepper &stepper, bool (*isTriggered)(), float stepsPerMm, float feedrate, float minRate,
                  float maxRate, int32_t *stepsPositionTarget);

    /**
     * @brief Sposta i cursori fuori dall'area degli endstop.
     * @param left True se i cursori sono bloccati a sinistra, false se a destra
     */
    void releaseFromEndstops(bool left);

    /**
     * @brief Avvia il processo di homing per ogni asse
     */
    void homeAllAxes();

    /**
     * @brief Inizia la diagnosi di un asse, conta gli step effettuati in ogni direzione ad una certa velocità
     * @param stepper Stepper dell'asse
     * @param stepsPerMm Step per millimetro
     * @param feedrate Velocità di movimento (mm/min)
     * @param minRate Minima velocità di movimento (mm/min)
     * @param maxRate Massima velocità di movimento (mm/min)
     */
    void diagnoseAxis(A4988Stepper &stepper, bool (*isTriggered)(), float stepsPerMm, int bounceSteps, float feedrate,
                      float minRate,
                      float maxRate);

    position::Position getPosition();
}
