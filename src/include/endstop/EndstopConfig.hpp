#pragma once

// Nei test su scheda simulata i finecorsa vengono abilitati (-DSIM_ENDSTOPS) per
// poter verificare homing e protezioni; sull'hardware valgono i valori qui sotto.
#ifdef SIM_ENDSTOPS
#define ENDSTOP_DEFAULT_ENABLED true
#else
#define ENDSTOP_DEFAULT_ENABLED false
#endif

namespace EndstopConfig {
    constexpr bool ENDSTOP_ENABLED_X = ENDSTOP_DEFAULT_ENABLED;
    constexpr bool ENDSTOP_ENABLED_Y = ENDSTOP_DEFAULT_ENABLED;
    constexpr bool ENDSTOP_ENABLED_Z = ENDSTOP_DEFAULT_ENABLED;

    constexpr bool ENDSTOP_INVERTED_X = true; // true = NC (inverso), false = NO
    constexpr bool ENDSTOP_INVERTED_Y = true;
    constexpr bool ENDSTOP_INVERTED_Z = true;
}
