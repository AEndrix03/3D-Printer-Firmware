#pragma once

namespace ExtruderConfig {
    constexpr float STEPS_PER_MM_E = 80.0f;

    constexpr float MAX_FEEDRATE_E = 2000.0f; // mm/min
    constexpr float MIN_FEEDRATE_E = 100.0f;

    constexpr float MANUAL_STEP_FEEDRATE = 300.0f; // mm/min, usato da A13
}
