#pragma once

namespace ExtruderController {
    void init();

    void move(float mm, float feedrate);

    void handle(int code, const char *params);

    void emergencyStop();
}
