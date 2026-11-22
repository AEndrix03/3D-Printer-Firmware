#pragma once

#include <stdint.h>
#include "MotionConfig.hpp"

namespace HomingConfig {
    constexpr float HOMING_X_FEEDRATE = 600.0f; // mm/min
    constexpr float HOMING_Y_FEEDRATE = 600.0f;
    constexpr float HOMING_Z_FEEDRATE = 200.0f;

    // Step di allontanamento dal finecorsa dopo il contatto
    constexpr int32_t HOMING_X_BOUNCE_OFFSET = 100;
    constexpr int32_t HOMING_Y_BOUNCE_OFFSET = 100;
    constexpr int32_t HOMING_Z_BOUNCE_OFFSET = 50;

    // Corsa massima (mm) entro cui il finecorsa deve scattare, altrimenti l'homing fallisce.
    // Piu' lunga del volume di lavoro: l'homing deve riuscire anche partendo dal bordo opposto.
    constexpr float HOMING_X_MAX_TRAVEL = MotionConfig::TRAVEL_MAX_X + 20.0f;
    constexpr float HOMING_Y_MAX_TRAVEL = MotionConfig::TRAVEL_MAX_Y + 20.0f;
    constexpr float HOMING_Z_MAX_TRAVEL = MotionConfig::TRAVEL_MAX_Z + 20.0f;

    // Step massimi percorsi da M99 (diagnosi asse) in ciascuna direzione
    constexpr int32_t DIAGNOSE_MAX_STEPS = 50000;
}
