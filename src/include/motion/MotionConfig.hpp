#pragma once

#include <stdint.h>

namespace MotionConfig {
    constexpr float STEPS_PER_MM_X = 80.0f;
    constexpr float STEPS_PER_MM_Y = 80.0f;
    constexpr float STEPS_PER_MM_Z = 400.0f;

    constexpr float MAX_FEEDRATE_X = 5000.0f; // mm/min
    constexpr float MAX_FEEDRATE_Y = 5000.0f;
    constexpr float MAX_FEEDRATE_Z = 150.0f;

    // Volume di lavoro (mm), misurato dal finecorsa (zero). Un movimento che porterebbe
    // un asse riferito fuori da [0, TRAVEL_MAX] viene rifiutato con "ERR LIMIT".
    // VERIFICARE sulla macchina reale.
    constexpr bool SOFT_LIMITS_ENABLED = true;
    constexpr float TRAVEL_MAX_X = 220.0f;
    constexpr float TRAVEL_MAX_Y = 220.0f;
    constexpr float TRAVEL_MAX_Z = 200.0f;

    constexpr float MIN_FEEDRATE = 30.0f;     // mm/min lungo il percorso

    // Accelerazioni massime per asse (mm/s^2). Un movimento che coinvolge piu' assi
    // usa la piu' alta accelerazione che non supera il limite di nessuno.
    constexpr float ACCELERATION_XY = 1000.0f;
    constexpr float ACCELERATION_Z = 100.0f;
    constexpr float ACCELERATION_E = 1500.0f;

    // Quanto il percorso puo' "tagliare" uno spigolo (mm): determina la velocita'
    // in curva. Piu' basso = spigoli piu' fedeli ma piu' lenti. 0.02 e' un valore tipico.
    constexpr float JUNCTION_DEVIATION = 0.02f;

    // Limiti del generatore di step. 8 kHz = 100 mm/s a 80 step/mm (oltre i 5000 mm/min di MAX_FEEDRATE): l'interrupt
    // impiega ~60-70 us nel caso peggiore (in rampa) su ATmega328P a 16 MHz.
    constexpr uint16_t MAX_STEP_RATE = 8000;
    constexpr uint16_t MIN_STEP_RATE = 40;
}
