#pragma once

namespace ExtruderController {
    void init();

    void move(float mm, float feedrate);

    void retract(float mm, float feedrate);

    void purge(float mm, float feedrate);

    void stepManual(int steps, bool direction);

    void resetPosition();

    void setStepsPerMM(float newSteps);


    void handle(int code, const char *params);

    void emergencyStop();
}
