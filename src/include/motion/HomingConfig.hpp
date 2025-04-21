#pragma once

namespace HomingConfig {
    constexpr float HOMING_X_FEEDRATE = 1000.0f;
    constexpr float HOMING_Y_FEEDRATE = 1000.0f;
    constexpr float HOMING_Z_FEEDRATE = 600.0f;

    constexpr float HOMING_X_BOUNCE_OFFSET = 5.0f;
    constexpr float HOMING_Y_BOUNCE_OFFSET = 5.0f;
    constexpr float HOMING_Z_BOUNCE_OFFSET = 10.0f;
    constexpr float HOMING_BOUNCE_FEEDRATE = 600.0f;

    constexpr int HOMING_MAX_ERROR_STEPS = 1000;
}
