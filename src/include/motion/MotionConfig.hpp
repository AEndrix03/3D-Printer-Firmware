#pragma once

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
}
