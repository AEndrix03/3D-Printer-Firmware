#pragma once

#include <stdint.h>

namespace position {

    /// Posizione in step. int32_t: su AVR "int" e' a 16 bit e l'asse Z
    /// (400 step/mm) andrebbe in overflow gia' a 81 mm.
    struct Position {
        int32_t x = 0;
        int32_t y = 0;
        int32_t z = 0;
    };

}
