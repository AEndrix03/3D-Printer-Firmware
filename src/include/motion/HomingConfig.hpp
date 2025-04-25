#pragma once

namespace HomingConfig {
    constexpr float HOMING_X_FEEDRATE = 600.0f;
    constexpr float HOMING_Y_FEEDRATE = 600.0f;
    constexpr float HOMING_Z_FEEDRATE = 200.0f;

    constexpr float HOMING_X_BOUNCE_OFFSET = 100.0f;
    constexpr float HOMING_Y_BOUNCE_OFFSET = 100.0f;
    constexpr float HOMING_Z_BOUNCE_OFFSET = 50.0f;
    constexpr float HOMING_BOUNCE_FEEDRATE = 600.0f;

    constexpr int HOMING_MAX_ERROR_STEPS = 1000;
}
