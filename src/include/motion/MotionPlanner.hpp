#pragma once

#include <stdint.h>

/**
 * @brief Intervallo tra step (microsecondi) per un asse a velocita' costante.
 * @param feedrate mm/min, limitato tra minFeedrate e maxFeedrate
 */
uint16_t computeDelayMicros(float feedrate, float stepsPerMm, float minFeedrate, float maxFeedrate);
