#include "../include/controllers/ExtruderController.hpp"
#include "../include/hal/drivers/stepper/A4988Stepper.hpp"
#include "../include/hal/drivers/StepperConfig.hpp"
#include "../include/Pins.hpp"
#include "../include/motion/MotionPlanner.hpp"
#include "../include/extruder/ExtruderConfig.hpp"
#include <math.h>
#include <Arduino.h>

namespace {
    A4988Stepper stepperE(PIN_E_STEP, PIN_E_DIR);
}

namespace ExtruderController {
    void init() {
        stepperE.init(StepperConfig::STEPPER_INVERT_E);
        stepperE.enable(StepperConfig::STEPPER_ENABLE_E);
    }

    void move(float mm, float feedrate) {
        Serial.print(F("EXTRUDING "));
        Serial.print(mm);
        Serial.print(F("mm @ "));
        Serial.print(feedrate);
        Serial.println(F(" mm/min"));

        MotionCommand cmd = {0, 0, 0, feedrate};
        cmd.x = mm;

        MotionPlan plan;
        plan.stepsX = (int) (fabs(mm) * ExtruderConfig::STEPS_PER_MM_E + 0.5f);
        plan.dirX = mm >= 0;
        plan.delayMicrosX = computeDelayMicros(feedrate, ExtruderConfig::MIN_FEEDRATE_E,
                                               ExtruderConfig::MAX_FEEDRATE_E);

        stepperE.setDirection(plan.dirX);
        for (int i = 0; i < plan.stepsX; ++i) {
            stepperE.step();
            delayMicroseconds(plan.delayMicrosX);
        }
    }

    void handle(int code, const char *params) {
        switch (code) {
            case 10: { // A10 E10.0 F800
                float mm = 0.0f, feedrate = 1000.0f;
                const char *pe = strchr(params, 'E');
                const char *pf = strchr(params, 'F');
                if (pe) mm = atof(pe + 1);
                if (pf) feedrate = atof(pf + 1);

                move(mm, feedrate);

                break;
            }
            case 0: {
                emergencyStop();
                break;
            }

            default:
                Serial.print(F("Unknown A-code for Extrusion: A"));
                Serial.println(code);
                break;
        }
    }

    void emergencyStop() {
        stepperE.enable(false);
    }
}
