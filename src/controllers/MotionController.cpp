#include <Arduino.h>
#include "../include/controllers/MotionController.hpp"
#include "../include/controllers/EndstopController.hpp"
#include "../include/controllers/ExtruderController.hpp"
#include "../include/StateMachine.hpp"
#include "../include/Pins.hpp"
#include "../include/SafetyManager.hpp"
#include "../include/motion/MotionConfig.hpp"
#include "../include/motion/HomingConfig.hpp"
#include "../include/motion/MotionPlanner.hpp"
#include "../include/endstop/EndstopConfig.hpp"
#include "../include/extruder/ExtruderConfig.hpp"
#include "../include/hal/drivers/StepperConfig.hpp"
#include "../include/hal/drivers/stepper/A4988Stepper.hpp"
#include "../include/BusyHandler.hpp"
#include "../include/ParamParser.hpp"
#include "../include/hal/McuHAL.hpp"
#include "../include/CompactResponse.hpp"

namespace {
    enum : uint8_t { AX_X = 0, AX_Y, AX_Z, AX_E, AX_COUNT };

    enum MoveResult : uint8_t { MOVE_DONE, MOVE_ABORTED, MOVE_ENDSTOP, MOVE_ERROR };

    A4988Stepper stepperX(PIN_X_STEP, PIN_X_DIR);
    A4988Stepper stepperY(PIN_Y_STEP, PIN_Y_DIR);
    A4988Stepper stepperZ(PIN_Z_STEP, PIN_Z_DIR);

    position::Position currentPosition;
    float eRemainder = 0.0f;     // frazione di step E non ancora eseguita (evita deriva di estrusione)
    float lastFeedrate = 1000.0f; // F e' modale
    bool abortRequested = false;

    /**
     * Lavori periodici durante un movimento bloccante: watchdog, BUSY, protezioni
     * e abort dall'host. Il protocollo e' stop-and-wait, quindi durante un
     * movimento l'unico byte valido in arrivo e' '!'.
     * @return false se il movimento deve fermarsi
     */
    bool housekeeping() {
        hal::watchdog::reset();
        BusyHandler::update();
        SafetyManager::update();
        while (hal::serial->available()) {
            if (hal::serial->read() == '!') abortRequested = true;
        }
        return !abortRequested && StateMachine::getState() != MachineState::Error;
    }

    /** Attende l'istante `next` (micros). Se si e' in ritardo non recupera con una raffica di step. */
    inline void waitUntil(uint32_t &next, uint32_t interval) {
        if ((int32_t) (hal::halMicros() - next) > (int32_t) interval) {
            next = hal::halMicros();
            return;
        }
        while ((int32_t) (hal::halMicros() - next) < 0) {}
    }

    inline void stepAxis(uint8_t axis, bool positive) {
        switch (axis) {
            case AX_X:
                stepperX.step();
                currentPosition.x += positive ? 1 : -1;
                break;
            case AX_Y:
                stepperY.step();
                currentPosition.y += positive ? 1 : -1;
                break;
            case AX_Z:
                stepperZ.step();
                currentPosition.z += positive ? 1 : -1;
                break;
            default:
                ExtruderController::stepRaw(positive);
                break;
        }
    }

    /**
     * Segmento rettilineo coordinato su 4 assi.
     * @param delta step con segno per X, Y, Z, E
     * @param feedrate velocita' lungo il percorso XYZ in mm/min (per E se il movimento e' di sola estrusione)
     */
    MoveResult runLine(const int32_t *delta, float feedrate) {
        uint32_t steps[AX_COUNT];
        uint32_t maxSteps = 0;
        uint8_t dominant = AX_X;

        for (uint8_t a = 0; a < AX_COUNT; ++a) {
            steps[a] = (uint32_t) labs(delta[a]);
            if (steps[a] > maxSteps) {
                maxSteps = steps[a];
                dominant = a;
            }
        }
        if (maxSteps == 0) return MOVE_DONE;

        // --- velocita': F vale lungo il percorso, limitata dal massimo di ogni asse ---
        const float stepsPerMm[AX_COUNT] = {
                MotionConfig::STEPS_PER_MM_X, MotionConfig::STEPS_PER_MM_Y,
                MotionConfig::STEPS_PER_MM_Z, ExtruderController::getStepsPerMm()
        };
        const float maxFeed[AX_COUNT] = {
                MotionConfig::MAX_FEEDRATE_X, MotionConfig::MAX_FEEDRATE_Y,
                MotionConfig::MAX_FEEDRATE_Z, ExtruderConfig::MAX_FEEDRATE_E
        };

        float mm[AX_COUNT];
        float length = 0.0f;
        for (uint8_t a = 0; a < AX_COUNT; ++a) {
            mm[a] = (float) steps[a] / stepsPerMm[a];
            if (a != AX_E) length += mm[a] * mm[a];
        }
        length = sqrt(length);
        if (length < 0.0001f) length = mm[AX_E]; // sola estrusione

        if (feedrate < MotionConfig::MIN_FEEDRATE_Z) feedrate = MotionConfig::MIN_FEEDRATE_Z;
        for (uint8_t a = 0; a < AX_COUNT; ++a) {
            if (mm[a] > 0.0f) {
                const float limit = maxFeed[a] * length / mm[a];
                if (feedrate > limit) feedrate = limit;
            }
        }

        uint32_t cruise = (uint32_t) (60.0e6f * length / (feedrate * (float) maxSteps));
        if (cruise < MotionConfig::MIN_STEP_INTERVAL_US) cruise = MotionConfig::MIN_STEP_INTERVAL_US;
        if (cruise > 60000UL) cruise = 60000UL;

        // --- rampa trapezoidale sull'asse dominante (ricorrenza intera, niente float per step) ---
        uint16_t startRate = MotionConfig::START_RATE_XY;
        uint32_t accel = MotionConfig::ACCEL_XY;
        if (dominant == AX_Z) {
            startRate = MotionConfig::START_RATE_Z;
            accel = MotionConfig::ACCEL_Z;
        } else if (dominant == AX_E) {
            startRate = MotionConfig::START_RATE_E;
            accel = MotionConfig::ACCEL_E;
        }

        const uint32_t startInterval = 1000000UL / startRate;
        uint32_t interval = cruise;
        uint32_t rampSteps = 0;
        uint32_t rampBase = 0; // step "virtuali" gia' percorsi per arrivare a startRate: n = v^2 / 2a

        if (cruise < startInterval) {
            interval = startInterval;
            const float cruiseRate = 1.0e6f / (float) cruise;
            rampBase = (uint32_t) (((float) startRate * (float) startRate) / (2.0f * (float) accel));
            const uint32_t cruiseN = (uint32_t) ((cruiseRate * cruiseRate) / (2.0f * (float) accel));
            rampSteps = cruiseN > rampBase ? cruiseN - rampBase : 0;
            if (rampSteps > maxSteps / 2) rampSteps = maxSteps / 2;
        }

        // --- direzioni ---
        stepperX.setDirection(delta[AX_X] >= 0);
        stepperY.setDirection(delta[AX_Y] >= 0);
        stepperZ.setDirection(delta[AX_Z] >= 0);

        // I finecorsa sono sul lato minimo: fermano solo i movimenti verso il negativo
        const bool watchX = delta[AX_X] < 0;
        const bool watchY = delta[AX_Y] < 0;
        const bool watchZ = delta[AX_Z] < 0;

        uint32_t err[AX_COUNT];
        for (uint8_t a = 0; a < AX_COUNT; ++a) err[a] = maxSteps / 2;

        abortRequested = false;
        uint32_t next = hal::halMicros();

        for (uint32_t i = 0; i < maxSteps; ++i) {
            if ((i & 15) == 0 && !housekeeping()) {
                return abortRequested ? MOVE_ABORTED : MOVE_ERROR;
            }

            if ((watchX && EndstopController::isTriggeredX()) ||
                (watchY && EndstopController::isTriggeredY()) ||
                (watchZ && EndstopController::isTriggeredZ())) {
                return MOVE_ENDSTOP;
            }

            // Bresenham: ogni asse avanza in proporzione, il percorso resta una retta
            for (uint8_t a = 0; a < AX_COUNT; ++a) {
                err[a] += steps[a];
                if (err[a] >= maxSteps) {
                    err[a] -= maxSteps;
                    stepAxis(a, delta[a] >= 0);
                }
            }

            if (i < rampSteps) {
                const uint32_t n = rampBase + i + 1;
                interval -= (2 * interval) / (4 * n + 1);
                if (interval < cruise) interval = cruise;
            } else if (i >= maxSteps - rampSteps) {
                const uint32_t n = rampBase + (maxSteps - i);
                interval += (2 * interval) / (4 * n - 1);
                if (interval > startInterval) interval = startInterval;
            }

            next += interval;
            waitUntil(next, interval);
        }

        return MOVE_DONE;
    }

    /** Traduce l'esito di un movimento nella risposta al comando. */
    bool report(MoveResult result) {
        switch (result) {
            case MOVE_DONE:
                return true;
            case MOVE_ABORTED:
                CompactResponse::reject(PSTR("ABORTED"));
                break;
            case MOVE_ENDSTOP:
                CompactResponse::reject(PSTR("ENDSTOP"));
                break;
            default:
                CompactResponse::setResult(CompactResponse::BLOCKED_MOTION_ERROR);
                break;
        }
        return false;
    }

    /**
     * Muove un solo asse a velocita' costante finche' stop() e' vero o si raggiunge maxSteps.
     * @return step eseguiti, -1 se interrotto
     */
    int32_t runAxis(A4988Stepper &stepper, bool dir, uint16_t interval, int32_t maxSteps, bool (*stop)()) {
        stepper.setDirection(dir);
        abortRequested = false;
        uint32_t next = hal::halMicros();
        int32_t done = 0;

        while (done < maxSteps && !(stop && stop())) {
            if ((done & 15) == 0 && !housekeeping()) return -1;
            stepper.step();
            ++done;
            next += interval;
            waitUntil(next, interval);
        }
        return done;
    }

    void sendAxisEvent(PGM_P type, char axis, PGM_P event) {
        const char name[2] = {axis, '\0'};
        CompactResponse::begin(type);
        CompactResponse::addStr(name);
        CompactResponse::addP(event);
        CompactResponse::end();
    }

    bool homeAxis(char axis, A4988Stepper &stepper, bool endstopEnabled, bool (*isTriggered)(),
                  uint16_t interval, int32_t maxTravelSteps, int32_t bounceSteps, int32_t &position) {
        if (!endstopEnabled) {
            // Nessun finecorsa: la posizione attuale diventa l'origine
            position = 0;
            sendAxisEvent(PSTR("HOM"), axis, PSTR("SKIP"));
            return true;
        }

        sendAxisEvent(PSTR("HOM"), axis, PSTR("START"));

        if (!isTriggered()) {
            if (runAxis(stepper, false, interval, maxTravelSteps, isTriggered) < 0) return false;
            if (!isTriggered()) {
                sendAxisEvent(PSTR("HOM"), axis, PSTR("FAIL"));
                return false;
            }
        }

        if (runAxis(stepper, true, interval, bounceSteps, nullptr) < 0) return false;

        position = bounceSteps; // il finecorsa e' lo zero, ci si e' allontanati di bounceSteps
        sendAxisEvent(PSTR("HOM"), axis, PSTR("OK"));
        return true;
    }

    void diagnoseAxis(A4988Stepper &stepper, bool (*isTriggered)(), int32_t bounceSteps,
                      uint16_t interval, int32_t &position) {
        CompactResponse::sendData(PSTR("DIG"), PSTR("START"));

        const int32_t up = runAxis(stepper, true, interval, HomingConfig::DIAGNOSE_MAX_STEPS, isTriggered);
        if (up < 0) {
            report(abortRequested ? MOVE_ABORTED : MOVE_ERROR);
            return;
        }
        position += up;
        CompactResponse::begin(PSTR("DIG"));
        CompactResponse::addP(PSTR("UP"));
        CompactResponse::addInt(up);
        CompactResponse::end();

        const int32_t down = runAxis(stepper, false, interval, HomingConfig::DIAGNOSE_MAX_STEPS, isTriggered);
        if (down < 0) {
            report(abortRequested ? MOVE_ABORTED : MOVE_ERROR);
            return;
        }
        position -= down;

        // si allontana dal finecorsa
        const int32_t back = runAxis(stepper, true, interval, bounceSteps, nullptr);
        if (back > 0) position += back;

        CompactResponse::begin(PSTR("DIG"));
        CompactResponse::addP(PSTR("DN"));
        CompactResponse::addInt(down);
        CompactResponse::end();
        CompactResponse::sendData(PSTR("DIG"), PSTR("COMPLETE"));
    }

    void handleMove(bool absolute, const char *params) {
        if (StateMachine::getState() != MachineState::Printing) {
            CompactResponse::reject(PSTR("INV_STATE"));
            return;
        }

        int32_t delta[AX_COUNT] = {0, 0, 0, 0};
        float v;

        if (ParamParser::get(params, 'X', v)) {
            delta[AX_X] = lround(v * MotionConfig::STEPS_PER_MM_X) - (absolute ? currentPosition.x : 0);
        }
        if (ParamParser::get(params, 'Y', v)) {
            delta[AX_Y] = lround(v * MotionConfig::STEPS_PER_MM_Y) - (absolute ? currentPosition.y : 0);
        }
        if (ParamParser::get(params, 'Z', v)) {
            delta[AX_Z] = lround(v * MotionConfig::STEPS_PER_MM_Z) - (absolute ? currentPosition.z : 0);
        }
        if (ParamParser::get(params, 'E', v) && v != 0.0f) {
            if (!ExtruderController::canExtrude()) {
                CompactResponse::reject(PSTR("COLD_EXT"));
                return;
            }
            // E e' sempre relativo; il resto frazionario viene riportato al movimento successivo
            const float exact = v * ExtruderController::getStepsPerMm() + eRemainder;
            delta[AX_E] = lround(exact);
            eRemainder = exact - (float) delta[AX_E];
        }
        if (ParamParser::get(params, 'F', v) && v > 0.0f) {
            lastFeedrate = v;
        }

        report(runLine(delta, lastFeedrate));
    }
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

    void setEnabled(bool enabled) {
        stepperX.enable(enabled && StepperConfig::STEPPER_ENABLE_X);
        stepperY.enable(enabled && StepperConfig::STEPPER_ENABLE_Y);
        stepperZ.enable(enabled && StepperConfig::STEPPER_ENABLE_Z);
    }

    void emergencyStop() {
        setEnabled(false);
        CompactResponse::sendData(PSTR("STP"), PSTR("EMG"));
    }

    bool moveExtruder(int32_t steps, float feedrate) {
        const int32_t delta[AX_COUNT] = {0, 0, 0, steps};
        return report(runLine(delta, feedrate));
    }

    position::Position getPosition() {
        return currentPosition;
    }

    void handle(uint8_t code, const char *params) {
        if (StateMachine::getState() == MachineState::Error && code != 0 && code != 114) {
            CompactResponse::begin(PSTR("ERR"));
            CompactResponse::addStr(SafetyManager::getErrorReason());
            CompactResponse::end();
            CompactResponse::setResult(CompactResponse::BLOCKED_MOTION_ERROR);
            return;
        }

        switch (code) {
            case 10:
                handleMove(false, params);
                break;
            case 11:
                handleMove(true, params);
                break;
            case 12: {
                float v;
                if (ParamParser::get(params, 'X', v)) currentPosition.x = lround(v * MotionConfig::STEPS_PER_MM_X);
                if (ParamParser::get(params, 'Y', v)) currentPosition.y = lround(v * MotionConfig::STEPS_PER_MM_Y);
                if (ParamParser::get(params, 'Z', v)) currentPosition.z = lround(v * MotionConfig::STEPS_PER_MM_Z);
                break;
            }
            case 99: {
                float feedrate = 800.0f;
                ParamParser::get(params, 'F', feedrate);

                if (strchr(params, 'X')) {
                    diagnoseAxis(stepperX, EndstopController::isTriggeredX, HomingConfig::HOMING_X_BOUNCE_OFFSET,
                                 computeDelayMicros(feedrate, MotionConfig::STEPS_PER_MM_X,
                                                    MotionConfig::MIN_FEEDRATE_X, MotionConfig::MAX_FEEDRATE_X),
                                 currentPosition.x);
                } else if (strchr(params, 'Y')) {
                    diagnoseAxis(stepperY, EndstopController::isTriggeredY, HomingConfig::HOMING_Y_BOUNCE_OFFSET,
                                 computeDelayMicros(feedrate, MotionConfig::STEPS_PER_MM_Y,
                                                    MotionConfig::MIN_FEEDRATE_Y, MotionConfig::MAX_FEEDRATE_Y),
                                 currentPosition.y);
                } else if (strchr(params, 'Z')) {
                    diagnoseAxis(stepperZ, EndstopController::isTriggeredZ, HomingConfig::HOMING_Z_BOUNCE_OFFSET,
                                 computeDelayMicros(feedrate, MotionConfig::STEPS_PER_MM_Z,
                                                    MotionConfig::MIN_FEEDRATE_Z, MotionConfig::MAX_FEEDRATE_Z),
                                 currentPosition.z);
                } else {
                    CompactResponse::reject(PSTR("NO_AXIS"));
                }
                break;
            }
            case 114: {
                CompactResponse::begin(PSTR("POS"), true);
                CompactResponse::addFloat((float) currentPosition.x / MotionConfig::STEPS_PER_MM_X, 3);
                CompactResponse::addFloat((float) currentPosition.y / MotionConfig::STEPS_PER_MM_Y, 3);
                CompactResponse::addFloat((float) currentPosition.z / MotionConfig::STEPS_PER_MM_Z, 3);
                CompactResponse::end();
                break;
            }
            case 0:
                emergencyStop();
                break;
            default:
                CompactResponse::reject(PSTR("UNK_MCODE"));
                break;
        }
    }

    bool homeAllAxes() {
        const bool ok =
                homeAxis('X', stepperX, EndstopConfig::ENDSTOP_ENABLED_X, EndstopController::isTriggeredX,
                         computeDelayMicros(HomingConfig::HOMING_X_FEEDRATE, MotionConfig::STEPS_PER_MM_X,
                                            MotionConfig::MIN_FEEDRATE_X, MotionConfig::MAX_FEEDRATE_X),
                         (int32_t) (HomingConfig::HOMING_X_MAX_TRAVEL * MotionConfig::STEPS_PER_MM_X),
                         HomingConfig::HOMING_X_BOUNCE_OFFSET, currentPosition.x) &&
                homeAxis('Y', stepperY, EndstopConfig::ENDSTOP_ENABLED_Y, EndstopController::isTriggeredY,
                         computeDelayMicros(HomingConfig::HOMING_Y_FEEDRATE, MotionConfig::STEPS_PER_MM_Y,
                                            MotionConfig::MIN_FEEDRATE_Y, MotionConfig::MAX_FEEDRATE_Y),
                         (int32_t) (HomingConfig::HOMING_Y_MAX_TRAVEL * MotionConfig::STEPS_PER_MM_Y),
                         HomingConfig::HOMING_Y_BOUNCE_OFFSET, currentPosition.y) &&
                homeAxis('Z', stepperZ, EndstopConfig::ENDSTOP_ENABLED_Z, EndstopController::isTriggeredZ,
                         computeDelayMicros(HomingConfig::HOMING_Z_FEEDRATE, MotionConfig::STEPS_PER_MM_Z,
                                            MotionConfig::MIN_FEEDRATE_Z, MotionConfig::MAX_FEEDRATE_Z),
                         (int32_t) (HomingConfig::HOMING_Z_MAX_TRAVEL * MotionConfig::STEPS_PER_MM_Z),
                         HomingConfig::HOMING_Z_BOUNCE_OFFSET, currentPosition.z);

        if (!ok) {
            if (abortRequested) {
                report(MOVE_ABORTED);
            } else if (StateMachine::getState() == MachineState::Error) {
                report(MOVE_ERROR);
            } else {
                CompactResponse::reject(PSTR("HOME_FAIL"));
            }
        }
        return ok;
    }
}
