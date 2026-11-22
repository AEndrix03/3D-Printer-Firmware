#pragma once

#include <stdint.h>

/**
 * @brief Estrusore (categoria 'A').
 *
 *  - 0:  emergency stop
 *  - 10: estrusione relativa (E mm, F mm/min; E negativo = ritrazione)
 *  - 11: ritrazione di |E| mm
 *  - 12: spurgo di |E| mm
 *  - 13: step manuali (S step, D direzione)
 *  - 14: azzera la posizione E
 *  - 15: imposta gli step/mm (S)
 *  - 16: posizione E -> "EPO <mm>"
 */
namespace ExtruderController {
    void init();

    void handle(uint8_t code, const char *params);

    /** Esegue un singolo step (usato dal movimento coordinato). */
    void stepRaw(bool forward);

    float getStepsPerMm();

    /** true se la temperatura consente l'estrusione. */
    bool canExtrude();

    void emergencyStop();
}
