#pragma once

#include <stdint.h>

namespace MotionConfig {
    constexpr float STEPS_PER_MM_X = 80.0f;
    constexpr float STEPS_PER_MM_Y = 80.0f;
    constexpr float STEPS_PER_MM_Z = 400.0f;

    constexpr float MAX_FEEDRATE_X = 5000.0f; // mm/min
    constexpr float MAX_FEEDRATE_Y = 5000.0f;
    constexpr float MAX_FEEDRATE_Z = 150.0f;

    constexpr float MIN_FEEDRATE_X = 100.0f;
    constexpr float MIN_FEEDRATE_Y = 100.0f;
    constexpr float MIN_FEEDRATE_Z = 50.0f;

    // Intervallo minimo tra due step dell'asse dominante (limite del loop su AVR)
    constexpr uint16_t MIN_STEP_INTERVAL_US = 200;

    // Rampa di accelerazione, riferita all'asse dominante del movimento:
    // velocita' di partenza/arrivo (step/s) e accelerazione (step/s^2).
    // X/Y: 10 mm/s, 1000 mm/s^2 - Z: 1 mm/s, 50 mm/s^2 - E: 5 mm/s, 1000 mm/s^2
    constexpr uint16_t START_RATE_XY = 800;
    constexpr uint32_t ACCEL_XY = 80000UL;
    constexpr uint16_t START_RATE_Z = 400;
    constexpr uint32_t ACCEL_Z = 20000UL;
    constexpr uint16_t START_RATE_E = 400;
    constexpr uint32_t ACCEL_E = 80000UL;
}
