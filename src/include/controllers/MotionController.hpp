#pragma once

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
     * @param params Parametri associati (es. "X10.0 Y20.0 F1500")
     */
    void handle(int code, const char *params);

    /**
     * @brief Muove la testina/asse alla posizione specificata.
     * @param x Coordinata X (mm)
     * @param y Coordinata Y (mm)
     * @param f Velocità di movimento (feedrate, mm/min)
     */
    void moveTo(float x, float y, float f);

    /**
     * @brief Arresta immediatamente tutti i movimenti.
     */
    void emergencyStop();
}
