#include <Arduino.h>

#include "../include/controllers/MotionController.hpp"

#include "../include/controllers/EndstopController.hpp"
#include "../include/StateMachine.hpp"

#include "../include/Pins.hpp"
#include "../include/SafetyManager.hpp"
#include "../include/motion/MotionConfig.hpp"
#include "../include/motion/HomingConfig.hpp"
#include "../include/hal/drivers/StepperConfig.hpp"
#include "../include/hal/drivers/stepper/A4988Stepper.hpp"
#include "../include/motion/MotionPlanner.hpp"
#include "../include/BusyHandler.hpp"
#include "../include/hal/McuHAL.hpp"

#define REQUIRE_STATE(validState) if (StateMachine::getState() != validState) { Serial.println(F("INVALID STATE")); return; }
#define CHUNK_SIZE 64

namespace {
    A4988Stepper stepperX(PIN_X_STEP, PIN_X_DIR);
    A4988Stepper stepperY(PIN_Y_STEP, PIN_Y_DIR);
    A4988Stepper stepperZ(PIN_Z_STEP, PIN_Z_DIR);

    position::Position currentPosition = position::Position();
}

namespace MotionController {
    void init() {
        stepperX.init(StepperConfig::STEPPER_INVERT_X);
        stepperY.init(StepperConfig::STEPPER_INVERT_Y);
        stepperZ.init(StepperConfig::STEPPER_INVERT_Z);
        stepperX.enable(StepperConfig::STEPPER_ENABLE_X);
        stepperY.enable(StepperConfig::STEPPER_ENABLE_Y);
        stepperZ.enable(StepperConfig::STEPPER_ENABLE_Z);
    }

    void moveTo(float x, float y, float z, float f) {
        MotionCommand cmd = {x, y, z, f};
        MotionPlan plan = planMotion(cmd);

        stepperX.setDirection(plan.dirX);
        stepperY.setDirection(plan.dirY);
        stepperZ.setDirection(plan.dirZ);

        int maxSteps = plan.stepsX;
        if (plan.stepsY > maxSteps) maxSteps = plan.stepsY;
        if (plan.stepsZ > maxSteps) maxSteps = plan.stepsZ;

        for (int i = 0; i < maxSteps; ++i) {
            BusyHandler::update();

            /* ---- asse X ---- */
            if (i < plan.stepsX) {
                if (EndstopController::isTriggeredX()) break;
                stepperX.step();
                delayMicroseconds(plan.delayMicrosX);
                currentPosition.x += (plan.dirX ? 1 : -1);
            }
            /* ---- asse Y ---- */
            if (i < plan.stepsY) {
                if (EndstopController::isTriggeredY()) break;
                stepperY.step();
                delayMicroseconds(plan.delayMicrosY);
                currentPosition.y += (plan.dirY ? 1 : -1);
            }
            /* ---- asse Z ---- */
            if (i < plan.stepsZ) {
                if (EndstopController::isTriggeredZ()) break;
                stepperZ.step();
                delayMicroseconds(plan.delayMicrosZ);
                currentPosition.z += (plan.dirZ ? 1 : -1);
            }

            /* ---- Refresh wathdog & machine state ---- */
            if ((i & (CHUNK_SIZE - 1)) == 0) {
                hal::watchdog::reset();
                SafetyManager::update();
            }
        }
    }

    void goTo(float targetX, float targetY, float targetZ, float feedrate) {
        position::Position current = getPosition();

        float dx = ((targetX * MotionConfig::STEPS_PER_MM_X) - ((float) current.x)) / MotionConfig::STEPS_PER_MM_X;
        float dy = ((targetY * MotionConfig::STEPS_PER_MM_Y) - ((float) current.y)) / MotionConfig::STEPS_PER_MM_Y;
        float dz = ((targetZ * MotionConfig::STEPS_PER_MM_Z) - ((float) current.z)) / MotionConfig::STEPS_PER_MM_Z;

        moveTo(dx, dy, dz, feedrate);
    }


    void emergencyStop() {
        // Disabilita tutti i motori immediatamente
        stepperX.enable(false);
        stepperY.enable(false);
        stepperZ.enable(false);

        Serial.println(F("MOTION EMERGENCY STOP"));
    }

    position::Position getPosition() {
        return currentPosition;
    }

    void handle(uint8_t code, const char *params) {
        if (StateMachine::getState() == MachineState::Error && code != 0) {
            Serial.println(F("ERR MOTION_BLOCKED_ERROR_STATE"));
            Serial.print(F("Use S6 to clear error. Reason: "));
            Serial.println(SafetyManager::getErrorReason());
            return;
        }

        switch (code) {
            case 10: {
                REQUIRE_STATE(MachineState::Printing);
                float x = 0.0f, y = 0.0f, z = 0.0f, f = 1000.0f;
                const char *px = strchr(params, 'X');
                const char *py = strchr(params, 'Y');
                const char *pz = strchr(params, 'Z');
                const char *pf = strchr(params, 'F');
                if (px) x = atof(px + 1);
                if (py) y = atof(py + 1);
                if (pz) z = atof(pz + 1);
                if (pf) f = atof(pf + 1);
                moveTo(x, y, z, f);
                break;
            }
            case 11: {
                // Movimento assoluto: M11 X... Y... Z... F...
                REQUIRE_STATE(MachineState::Printing);
                float x = 0.0f, y = 0.0f, z = 0.0f, f = 1000.0f;
                const char *px = strchr(params, 'X');
                const char *py = strchr(params, 'Y');
                const char *pz = strchr(params, 'Z');
                const char *pf = strchr(params, 'F');
                if (px) x = atof(px + 1);
                if (py) y = atof(py + 1);
                if (pz) z = atof(pz + 1);
                if (pf) f = atof(pf + 1);
                goTo(x, y, z, f);
                break;
            }
            case 99: {
                // Esempio: M99 X F800
                const char *pa = strchr(params, 'X');
                const char *pb = strchr(params, 'Y');
                const char *pc = strchr(params, 'Z');
                const char *pf = strchr(params, 'F');

                float feedrate = 800.0f;
                if (pf) feedrate = atof(pf + 1);

                if (pa) {
                    diagnoseAxis(stepperX, EndstopController::isTriggeredX,
                                 HomingConfig::HOMING_X_BOUNCE_OFFSET,
                                 MotionConfig::STEPS_PER_MM_X,
                                 feedrate,
                                 MotionConfig::MIN_FEEDRATE_X,
                                 MotionConfig::MAX_FEEDRATE_X);
                } else if (pb) {
                    diagnoseAxis(stepperY, EndstopController::isTriggeredY,
                                 HomingConfig::HOMING_Y_BOUNCE_OFFSET,
                                 MotionConfig::STEPS_PER_MM_Y,
                                 feedrate,
                                 MotionConfig::MIN_FEEDRATE_Y,
                                 MotionConfig::MAX_FEEDRATE_Y);
                } else if (pc) {
                    diagnoseAxis(stepperZ, EndstopController::isTriggeredZ,
                                 HomingConfig::HOMING_Z_BOUNCE_OFFSET,
                                 MotionConfig::STEPS_PER_MM_Z,
                                 feedrate,
                                 MotionConfig::MIN_FEEDRATE_Z,
                                 MotionConfig::MAX_FEEDRATE_Z);
                } else {
                    Serial.println(F("M99 ERROR: specify axis X, Y or Z"));
                }

                break;
            }
            case 114: {
                // M114: stampa posizione corrente
                position::Position pos = getPosition();
                Serial.print(F("Position: X="));
                Serial.print((pos.x / MotionConfig::STEPS_PER_MM_X));
                Serial.print(F(" Y="));
                Serial.print((pos.y / MotionConfig::STEPS_PER_MM_Y));
                Serial.print(F(" Z="));
                Serial.println((pos.z / MotionConfig::STEPS_PER_MM_Z));
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

    void homeAxis(A4988Stepper &stepper, bool (*isTriggered)(), float speedPerMm, float feedrate, float minRate,
                  float maxRate, int *stepsPositionTarget) {
        if (isTriggered()) {
            Serial.println(F("ENDSTOP ALREADY TRIGGERED"));
            return;
        }

        stepper.setDirection(false);
        int bouceSteps = HomingConfig::HOMING_BOUNCE_FEEDRATE;
        int steps = 0;

        while (!isTriggered()) {
            (*stepsPositionTarget)--;
            BusyHandler::update();
            if ((steps & (CHUNK_SIZE - 1)) == 0) {
                hal::watchdog::reset();
                SafetyManager::update();
            }
            stepper.step();
            delayMicroseconds(computeDelayMicros(feedrate, speedPerMm, minRate, maxRate));
            steps++;
        }

        if (isTriggered()) {
            Serial.println(F("ENDSTOP HIT"));
        }

        steps = 0;
        stepper.setDirection(true);
        while (steps < bouceSteps) {
            (*stepsPositionTarget)--;
            BusyHandler::update();
            if ((steps & (CHUNK_SIZE - 1)) == 0) {
                hal::watchdog::reset();
                SafetyManager::update();
            }

            stepper.step();
            delayMicroseconds(computeDelayMicros(feedrate, speedPerMm, minRate, maxRate));
            steps++;
        }
    }

    void releaseFromEndstops(bool left) {
        moveTo((left ? 1.0f : -1.0f) * HomingConfig::HOMING_X_BOUNCE_OFFSET,
               (left ? 1.0f : -1.0f) * HomingConfig::HOMING_Y_BOUNCE_OFFSET,
               (left ? 1.0f : -1.0f) * HomingConfig::HOMING_Z_BOUNCE_OFFSET,
               HomingConfig::HOMING_BOUNCE_FEEDRATE);
    }

    void homeAllAxes() {
        REQUIRE_STATE(MachineState::Homing);

        Serial.println(F("HOMING X..."));
        homeAxis(stepperX, EndstopController::isTriggeredX, MotionConfig::STEPS_PER_MM_X,
                 HomingConfig::HOMING_X_FEEDRATE,
                 MotionConfig::MIN_FEEDRATE_X, MotionConfig::MAX_FEEDRATE_X, &currentPosition.x);

        Serial.println(F("HOMING Y..."));
        homeAxis(stepperY, EndstopController::isTriggeredY, MotionConfig::STEPS_PER_MM_Y,
                 HomingConfig::HOMING_Y_FEEDRATE,
                 MotionConfig::MIN_FEEDRATE_Y, MotionConfig::MAX_FEEDRATE_Y, &currentPosition.y);

        Serial.println(F("HOMING Z..."));
        homeAxis(stepperZ, EndstopController::isTriggeredZ, MotionConfig::STEPS_PER_MM_Z,
                 HomingConfig::HOMING_Z_FEEDRATE,
                 MotionConfig::MIN_FEEDRATE_Z, MotionConfig::MAX_FEEDRATE_Z, &currentPosition.z);

        Serial.println(F("HOMING COMPLETE"));
    }

    void diagnoseAxis(A4988Stepper &stepper, bool (*isTriggered)(), float stepsPerMm, int bounceSteps, float feedrate,
                      float minRate,
                      float maxRate) {
        const float delayMicros = computeDelayMicros(feedrate, stepsPerMm, minRate, maxRate);
        Serial.println(F("Diagnosing axis..."));

        // SALITA (verso endstop)
        stepper.setDirection(true);
        int upSteps = 0;
        while (!isTriggered() && upSteps < 50000) {
            BusyHandler::update();
            stepper.step();
            delayMicroseconds(delayMicros);
            upSteps++;
        }

        Serial.print(F("UP steps until endstop: "));
        Serial.println(upSteps + bounceSteps);

        delay(200);
        stepper.setDirection(false);
        for (int i = 0; i < bounceSteps; ++i) {
            BusyHandler::update();
            stepper.step();
            delayMicroseconds(delayMicros);
        }
        delay(200);

        int downSteps = 0;
        while (!isTriggered() && downSteps < 50000) {
            BusyHandler::update();
            stepper.step();
            delayMicroseconds(delayMicros);
            downSteps++;
        }

        delay(200);
        stepper.setDirection(true);
        for (int i = 0; i < bounceSteps; ++i) {
            BusyHandler::update();
            stepper.step();
            delayMicroseconds(delayMicros);
        }
        delay(200);

        Serial.print(F("DOWN steps until endstop: "));
        Serial.println(downSteps + bounceSteps);

        Serial.println(F("Diagnosis complete"));
    }
}
