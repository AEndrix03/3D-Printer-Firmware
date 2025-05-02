#pragma once

#include <Arduino.h>

namespace position {

    struct Position {
        int32_t x = 0;
        int32_t y = 0;
        int32_t z = 0;
    };

}