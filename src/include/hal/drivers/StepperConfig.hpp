#pragma once

namespace StepperConfig {
    // Livello del pin di enable che alimenta i motori: LOW per A4988/DRV8825/TMC
    constexpr bool ENABLE_ACTIVE_LOW = true;

    constexpr bool STEPPER_INVERT_X = true;
    constexpr bool STEPPER_INVERT_Y = false;
    constexpr bool STEPPER_INVERT_Z = true;
    constexpr bool STEPPER_INVERT_E = false;
}

