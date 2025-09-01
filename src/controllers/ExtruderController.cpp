#include "../include/controllers/ExtruderController.hpp"
#include "../include/hal/drivers/stepper/A4988Stepper.hpp"
#include "../include/hal/drivers/StepperConfig.hpp"
#include "../include/Pins.hpp"
#include "../include/motion/MotionPlanner.hpp"
#include "../include/extruder/ExtruderConfig.hpp"
#include "../include/BusyHandler.hpp"
#include "../include/CompactResponse.hpp"
#include <math.h>
#include <Arduino.h>

namespace {
    A4988Stepper stepperE(PIN_E_STEP, PIN_E_DIR);
    float currentPositionE = 0.0f;
    float stepsPerMm = ExtruderConfig::STEPS_PER_MM_E;
}

namespace ExtruderController {
    void init() {
        stepperE.init(StepperConfig::STEPPER_INVERT_E);
        stepperE.enable(StepperConfig::STEPPER_ENABLE_E);
    }

    void move(float mm, float feedrate) {
        char moveData[32];
        snprintf(moveData, sizeof(moveData), "%.2f %.0f", mm, feedrate);
        CompactResponse::sendData("EXT", moveData);

        MotionCommand cmd = {0, 0, 0, feedrate};
        cmd.x = mm;

        MotionPlan plan;
        plan.stepsX = (int) (fabs(mm) * ExtruderConfig::STEPS_PER_MM_E + 0.5f);
        plan.dirX = mm >= 0;
        plan.delayMicrosX = computeDelayMicros(feedrate, ExtruderConfig::STEPS_PER_MM_E, ExtruderConfig::MIN_FEEDRATE_E,
                                               ExtruderConfig::MAX_FEEDRATE_E);

        stepperE.setDirection(plan.dirX);
        for (int i = 0; i < plan.stepsX; ++i) {
            BusyHandler::update();
            stepperE.step();
            delayMicroseconds(plan.delayMicrosX);
        }
    }

    void retract(float mm, float feedrate) {
        move(-fabs(mm), feedrate);
        currentPositionE -= fabs(mm);
    }

    void purge(float mm, float feedrate) {
        move(fabs(mm), feedrate);
        currentPositionE += fabs(mm);
    }

    void stepManual(int steps, bool direction) {
        char stepData[24];
        snprintf(stepData, sizeof(stepData), "%d %s", steps, direction ? "FWD" : "BWD");
        CompactResponse::sendData("STP", stepData);

        stepperE.setDirection(direction);
        for (int i = 0; i < steps; ++i) {
            BusyHandler::update();
            stepperE.step();
            delayMicroseconds(200);
        }

        if (direction) currentPositionE += steps / stepsPerMm;
        else currentPositionE -= steps / stepsPerMm;
    }

    void resetPosition() {
        currentPositionE = 0.0f;
        CompactResponse::sendData("RST", "0.0");
    }

    void setStepsPerMM(float newSteps) {
        stepsPerMm = newSteps;
        char stepsData[16];
        snprintf(stepsData, sizeof(stepsData), "%.2f", stepsPerMm);
        CompactResponse::sendData("SPM", stepsData);
    }

    void handle(uint8_t code, const char *params) {
        switch (code) {
            case 10: {
                float mm = 0.0f, feedrate = 1000.0f;
                const char *pe = strchr(params, 'E');
                const char *pf = strchr(params, 'F');
                if (pe) mm = atof(pe + 1);
                if (pf) feedrate = atof(pf + 1);
                move(mm, feedrate);
                break;
            }
            case 11: {
                float mm = 2.0f, feedrate = 1800.0f;
                const char *pe = strchr(params, 'E');
                const char *pf = strchr(params, 'F');
                if (pe) mm = atof(pe + 1);
                if (pf) feedrate = atof(pf + 1);
                retract(mm, feedrate);
                break;
            }
            case 12: {
                float mm = 5.0f, feedrate = 1000.0f;
                const char *pe = strchr(params, 'E');
                const char *pf = strchr(params, 'F');
                if (pe) mm = atof(pe + 1);
                if (pf) feedrate = atof(pf + 1);
                purge(mm, feedrate);
                break;
            }
            case 13: {
                int steps = 100;
                bool dir = true;
                const char *ps = strchr(params, 'S');
                const char *pd = strchr(params, 'D');
                if (ps) steps = atoi(ps + 1);
                if (pd) dir = atoi(pd + 1) != 0;
                stepManual(steps, dir);
                break;
            }
            case 14: {
                resetPosition();
                break;
            }
            case 15: {
                const char *ps = strchr(params, 'S');
                if (ps) {
                    float val = atof(ps + 1);
                    setStepsPerMM(val);
                }
                break;
            }
            case 16: {
                // A16: Get current position
                char posData[16];
                snprintf(posData, sizeof(posData), "%.2f", currentPositionE);
                CompactResponse::sendData("EPO", posData);
                break;
            }
            case 0: {
                emergencyStop();
                break;
            }
            default:
                CompactResponse::sendData("ERR", "UNK_ACODE");
                break;
        }
    }

    void emergencyStop() {
        stepperE.enable(false);
        CompactResponse::sendData("STP", "EMG");
    }
}