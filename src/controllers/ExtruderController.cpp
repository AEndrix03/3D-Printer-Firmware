#include <Arduino.h>
#include "../include/controllers/ExtruderController.hpp"
#include "../include/controllers/MotionController.hpp"
#include "../include/controllers/TempController.hpp"
#include "../include/hal/drivers/stepper/A4988Stepper.hpp"
#include "../include/hal/drivers/StepperConfig.hpp"
#include "../include/Pins.hpp"
#include "../include/extruder/ExtruderConfig.hpp"
#include "../include/ParamParser.hpp"
#include "../include/CompactResponse.hpp"

namespace {
    A4988Stepper stepperE(PIN_E_STEP, PIN_E_DIR);
    int32_t positionSteps = 0;
    float stepsPerMm = ExtruderConfig::STEPS_PER_MM_E;
    bool lastDirection = true;

    /** Estrusione relativa in mm, con controllo di estrusione a freddo. */
    void extrude(float mm, float feedrate) {
        if (mm == 0.0f) return;
        if (!ExtruderController::canExtrude()) {
            CompactResponse::reject(PSTR("COLD_EXT"));
            return;
        }
        MotionController::moveExtruder(lround(mm * stepsPerMm), feedrate);
    }
}

namespace ExtruderController {
    void init() {
        stepperE.init(StepperConfig::STEPPER_INVERT_E);
        stepperE.enable(StepperConfig::STEPPER_ENABLE_E);
        stepperE.setDirection(lastDirection);
    }

    void stepRaw(bool forward) {
        if (forward != lastDirection) {
            stepperE.setDirection(forward);
            lastDirection = forward;
        }
        stepperE.step();
        positionSteps += forward ? 1 : -1;
    }

    float getStepsPerMm() {
        return stepsPerMm;
    }

    bool canExtrude() {
        return TempController::canExtrude();
    }

    void handle(uint8_t code, const char *params) {
        float mm = 0.0f;
        float feedrate = 1000.0f;

        switch (code) {
            case 10:
                ParamParser::get(params, 'E', mm);
                ParamParser::get(params, 'F', feedrate);
                extrude(mm, feedrate);
                break;
            case 11:
                mm = 2.0f;
                feedrate = 1800.0f;
                ParamParser::get(params, 'E', mm);
                ParamParser::get(params, 'F', feedrate);
                extrude(-fabs(mm), feedrate);
                break;
            case 12:
                mm = 5.0f;
                ParamParser::get(params, 'E', mm);
                ParamParser::get(params, 'F', feedrate);
                extrude(fabs(mm), feedrate);
                break;
            case 13: {
                float steps = 100.0f;
                float dir = 1.0f;
                ParamParser::get(params, 'S', steps);
                ParamParser::get(params, 'D', dir);
                if (!canExtrude()) {
                    CompactResponse::reject(PSTR("COLD_EXT"));
                    break;
                }
                MotionController::moveExtruder(dir != 0.0f ? (int32_t) steps : -(int32_t) steps,
                                               ExtruderConfig::MANUAL_STEP_FEEDRATE);
                break;
            }
            case 14:
                positionSteps = 0;
                break;
            case 15: {
                float value = 0.0f;
                if (!ParamParser::get(params, 'S', value) || value <= 0.0f) {
                    CompactResponse::reject(PSTR("BAD_SPM"));
                    break;
                }
                stepsPerMm = value;
                CompactResponse::begin(PSTR("SPM"));
                CompactResponse::addFloat(stepsPerMm, 2);
                CompactResponse::end();
                break;
            }
            case 16:
                CompactResponse::begin(PSTR("EPO"));
                CompactResponse::addFloat((float) positionSteps / stepsPerMm, 3);
                CompactResponse::end();
                break;
            case 0:
                emergencyStop();
                break;
            default:
                CompactResponse::reject(PSTR("UNK_ACODE"));
                break;
        }
    }

    void emergencyStop() {
        stepperE.enable(false);
        CompactResponse::sendData(PSTR("STP"), PSTR("EMG"));
    }
}
