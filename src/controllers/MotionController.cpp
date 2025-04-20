#include <Arduino.h>
#include "../include/controllers/MotionController.hpp"
#include "../include/Pins.hpp"
#include "../include/hal/drivers/stepper/A4988Stepper.hpp"
#include "../include/motion/MotionPlanner.hpp"

namespace {
    A4988Stepper stepperX(PIN_X_STEP, PIN_X_DIR);
    A4988Stepper stepperY(PIN_Y_STEP, PIN_Y_DIR);
    A4988Stepper stepperZ(PIN_Z_STEP, PIN_Z_DIR);
}

namespace MotionController {
    void init() {
        stepperX.init();
        stepperY.init();
        stepperZ.init();
        stepperX.enable(true);
        stepperY.enable(true);
        stepperZ.enable(true);
    }

    void moveTo(float x, float y, float z, float f) {
        Serial.print(F("MOVE to X="));
        Serial.print(x);
        Serial.print(F(" Y="));
        Serial.print(y);
        Serial.print(F(" Z="));
        Serial.print(z);
        Serial.print(F(" F="));
        Serial.println(f);

        MotionCommand cmd = {x, y, z, f};
        MotionPlan plan = planMotion(cmd);

        stepperX.setDirection(plan.dirX);
        stepperY.setDirection(plan.dirY);
        stepperZ.setDirection(plan.dirZ);

        int maxSteps = plan.stepsX;
        if (plan.stepsY > maxSteps) maxSteps = plan.stepsY;
        if (plan.stepsZ > maxSteps) maxSteps = plan.stepsZ;

        for (int i = 0; i < maxSteps; ++i) {
            if (i < plan.stepsX)
                stepperX.
                        step();
            if (i < plan.stepsY)
                stepperY.
                        step();
            if (i < plan.stepsZ)
                stepperZ.
                        step();
            delayMicroseconds(plan.delayMicros);
        }
    }


    void emergencyStop() {
        stepperX.enable(false);
        stepperY.enable(false);
        Serial.println(F("EMERGENCY STOP triggered"));
    }

    void handle(int code, const char *params) {
        switch (code) {
            case 10: {
                float x = 0.0f, y = 0.0f, z = 0.0f, f = 1000.0f;
                const char *px = strchr(params, 'X');
                const char *py = strchr(params, 'Y');
                const char *pz = strchr(params, 'Z');
                const char *pf = strchr(params, 'F');
                if (px) x = atof(px + 1);
                if (py) y = atof(py + 1);
                if (py) z = atof(pz + 1);
                if (pf) f = atof(pf + 1);
                moveTo(x, y, z, f);
                break;
            }
            case 0:
                emergencyStop();
                break;
            default:
                Serial.print(F("Unknown M-code: M"));
                Serial.println(code);
                break;
        }
    }
}
