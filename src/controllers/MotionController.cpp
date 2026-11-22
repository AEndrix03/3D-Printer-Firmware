#include <Arduino.h>
#include "../include/controllers/MotionController.hpp"
#include "../include/controllers/EndstopController.hpp"
#include "../include/controllers/ExtruderController.hpp"
#include "../include/StateMachine.hpp"
#include "../include/Pins.hpp"
#include "../include/SafetyManager.hpp"
#include "../include/motion/MotionConfig.hpp"
#include "../include/motion/HomingConfig.hpp"
#include "../include/motion/Planner.hpp"
#include "../include/motion/Stepper.hpp"
#include "../include/endstop/EndstopConfig.hpp"
#include "../include/hal/drivers/StepperConfig.hpp"
#include "../include/hal/drivers/stepper/A4988Stepper.hpp"
#include "../include/BusyHandler.hpp"
#include "../include/ParamParser.hpp"
#include "../include/hal/McuHAL.hpp"
#include "../include/CompactResponse.hpp"

namespace {
    using Planner::AX_X;
    using Planner::AX_Y;
    using Planner::AX_Z;
    using Planner::AX_E;
    using Planner::AX_COUNT;

    // Usati solo per configurare i pin: gli step li genera lo Stepper in interrupt
    A4988Stepper stepperX(PIN_X_STEP, PIN_X_DIR);
    A4988Stepper stepperY(PIN_Y_STEP, PIN_Y_DIR);
    A4988Stepper stepperZ(PIN_Z_STEP, PIN_Z_DIR);

    float eRemainder = 0.0f;      // frazione di step E non ancora eseguita (evita deriva di estrusione)
    float lastFeedrate = 1000.0f; // F e' modale
    bool abortRequested = false;
    bool referenced = false;
    bool motorsOn = false;
    uint8_t limitAxes = 0;        // bit per asse: posizione riferita, i limiti di corsa valgono

    const float STEPS_PER_MM[3] = {MotionConfig::STEPS_PER_MM_X, MotionConfig::STEPS_PER_MM_Y,
                                   MotionConfig::STEPS_PER_MM_Z};

    const int32_t TRAVEL_MAX_STEPS[3] PROGMEM = {
            (int32_t) (MotionConfig::TRAVEL_MAX_X * MotionConfig::STEPS_PER_MM_X),
            (int32_t) (MotionConfig::TRAVEL_MAX_Y * MotionConfig::STEPS_PER_MM_Y),
            (int32_t) (MotionConfig::TRAVEL_MAX_Z * MotionConfig::STEPS_PER_MM_Z)};

    /**
     * Limiti software di corsa: valgono solo per gli assi con posizione riferita.
     * Un asse gia' fuori dal volume puo' sempre rientrare.
     */
    bool withinTravel(const int32_t *delta) {
        if (!MotionConfig::SOFT_LIMITS_ENABLED) return true;
        for (uint8_t a = 0; a < 3; ++a) {
            if (!(limitAxes & (1 << a)) || delta[a] == 0) continue;
            const int32_t target = Planner::position(a) + delta[a];
            const int32_t max = (int32_t) pgm_read_dword(&TRAVEL_MAX_STEPS[a]);
            if ((delta[a] < 0 && target < 0) || (delta[a] > 0 && target > max)) return false;
        }
        return true;
    }

    /** Dopo un arresto la coda e' stata svuotata: il planner riparte dalla posizione reale. */
    void resyncAfterStop() {
        if (!Stepper::consumeStopEvent()) return;
        for (uint8_t a = 0; a < AX_COUNT; ++a) Planner::setPosition(a, Stepper::position(a));
        eRemainder = 0.0f;
    }

    /**
     * Lavori periodici mentre un comando attende (posto in coda o fine dei movimenti):
     * watchdog, BUSY, protezioni e abort dall'host. Il protocollo e' stop-and-wait,
     * quindi durante l'attesa l'unico byte valido in arrivo e' '!'.
     * @return false se l'attesa va interrotta
     */
    bool waitTask() {
        hal::watchdog::reset();
        BusyHandler::update();
        SafetyManager::update();
        while (hal::serial->available()) {
            if (hal::serial->read() == '!') MotionController::abort();
        }
        return !abortRequested && StateMachine::getState() != MachineState::Error;
    }

    /** Risposta al comando quando un'attesa viene interrotta. */
    void reportInterrupted() {
        if (StateMachine::getState() == MachineState::Error) {
            CompactResponse::setResult(CompactResponse::BLOCKED_MOTION_ERROR);
        } else {
            CompactResponse::reject(PSTR("ABORTED"));
        }
    }

    /** Un arresto in corso deve concludersi prima di accettare altro. */
    void settle() {
        while (!Stepper::isIdle() && abortRequested) {
            hal::watchdog::reset();
            SafetyManager::update();
        }
        resyncAfterStop();
        abortRequested = false;
    }

    /** Accoda un segmento, aspettando un posto libero se serve. */
    bool enqueue(const int32_t *delta, float feedrate, uint32_t commandNumber, uint8_t flags = 0) {
        while (Planner::isFull()) {
            if (!waitTask()) {
                reportInterrupted();
                return false;
            }
        }
        // I motori spenti (emergenza rientrata, risparmio energetico) si riaccendono al primo movimento
        if (!motorsOn) MotionController::setEnabled(true);
        Planner::addLine(delta, feedrate, commandNumber, flags);
        return true;
    }

    void handleMove(bool absolute, const char *params, uint32_t commandNumber) {
        if (StateMachine::getState() != MachineState::Printing) {
            CompactResponse::reject(PSTR("INV_STATE"));
            return;
        }
        settle();

        int32_t delta[AX_COUNT] = {0, 0, 0, 0};
        float v;

        for (uint8_t a = 0; a < 3; ++a) {
            if (ParamParser::get(params, (char) ('X' + a), v)) {
                delta[a] = lround(v * STEPS_PER_MM[a]) - (absolute ? Planner::position(a) : 0);
            }
        }
        if (!withinTravel(delta)) {
            CompactResponse::reject(PSTR("LIMIT"));
            return;
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

        if (delta[AX_X] || delta[AX_Y] || delta[AX_Z] || delta[AX_E]) {
            enqueue(delta, lastFeedrate, commandNumber);
        }
    }

    /**
     * Muove un asse fino al finecorsa (o per maxSteps) e attende la fine.
     * @return step realmente percorsi, -1 se interrotto
     */
    int32_t seek(uint8_t axis, bool positive, int32_t maxSteps, float feedrate, bool untilEndstop,
                 uint32_t commandNumber) {
        const int32_t before = Stepper::position(axis);
        int32_t delta[AX_COUNT] = {0, 0, 0, 0};
        delta[axis] = positive ? maxSteps : -maxSteps;

        if (!enqueue(delta, feedrate, commandNumber, untilEndstop ? Planner::FLAG_SEEK_ENDSTOP : 0)) return -1;
        if (!MotionController::synchronize()) return -1;
        resyncAfterStop();

        return labs(Stepper::position(axis) - before);
    }

    void sendAxisEvent(PGM_P type, char axis, PGM_P event) {
        const char name[2] = {axis, '\0'};
        CompactResponse::begin(type);
        CompactResponse::addStr(name);
        CompactResponse::addP(event);
        CompactResponse::end();
    }

    bool homeAxis(uint8_t axis, bool endstopEnabled, bool (*isTriggered)(), float feedrate,
                  float maxTravelMm, int32_t bounceSteps) {
        const char name = (char) ('X' + axis);

        if (!endstopEnabled) {
            // Nessun finecorsa: la posizione attuale diventa l'origine
            Stepper::setPosition(axis, 0);
            Planner::setPosition(axis, 0);
            limitAxes |= (uint8_t) (1 << axis);
            sendAxisEvent(PSTR("HOM"), name, PSTR("SKIP"));
            return true;
        }

        sendAxisEvent(PSTR("HOM"), name, PSTR("START"));

        if (!isTriggered()) {
            if (seek(axis, false, (int32_t) (maxTravelMm * STEPS_PER_MM[axis]), feedrate, true, 0) < 0) return false;
            if (!Stepper::endstopHit()) {
                sendAxisEvent(PSTR("HOM"), name, PSTR("FAIL"));
                CompactResponse::setResult(CompactResponse::REJECTED_ERROR);
                return false;
            }
        }

        if (seek(axis, true, bounceSteps, feedrate, false, 0) < 0) return false;

        // il finecorsa e' lo zero: ci si e' allontanati di bounceSteps
        Stepper::setPosition(axis, bounceSteps);
        Planner::setPosition(axis, bounceSteps);
        limitAxes |= (uint8_t) (1 << axis);
        sendAxisEvent(PSTR("HOM"), name, PSTR("OK"));
        return true;
    }

    void diagnoseAxis(uint8_t axis, int32_t bounceSteps, float feedrate) {
        CompactResponse::sendData(PSTR("DIG"), PSTR("START"));

        const int32_t up = seek(axis, true, HomingConfig::DIAGNOSE_MAX_STEPS, feedrate, true, 0);
        if (up < 0) return;
        CompactResponse::begin(PSTR("DIG"));
        CompactResponse::addP(PSTR("UP"));
        CompactResponse::addInt(up);
        CompactResponse::end();

        const int32_t down = seek(axis, false, HomingConfig::DIAGNOSE_MAX_STEPS, feedrate, true, 0);
        if (down < 0) return;
        if (seek(axis, true, bounceSteps, feedrate, false, 0) < 0) return; // via dal finecorsa

        CompactResponse::begin(PSTR("DIG"));
        CompactResponse::addP(PSTR("DN"));
        CompactResponse::addInt(down);
        CompactResponse::end();
        CompactResponse::sendData(PSTR("DIG"), PSTR("COMPLETE"));
    }
}

namespace MotionController {
    void init() {
        stepperX.init(StepperConfig::STEPPER_INVERT_X);
        stepperY.init(StepperConfig::STEPPER_INVERT_Y);
        stepperZ.init(StepperConfig::STEPPER_INVERT_Z);

        abortRequested = false;
        referenced = false;
        limitAxes = 0;
        pinMode(PIN_STEPPERS_ENABLE, OUTPUT);
        setEnabled(true);
        eRemainder = 0.0f;
        Planner::init();
        Stepper::init();
    }

    void update() {
        resyncAfterStop();
    }

    void setEnabled(bool enabled) {
        digitalWrite(PIN_STEPPERS_ENABLE, enabled == StepperConfig::ENABLE_ACTIVE_LOW ? LOW : HIGH);
        motorsOn = enabled;
        if (!enabled) {
            // Senza corrente i motori possono essere spostati: la posizione non e' piu' affidabile
            referenced = false;
            limitAxes = 0;
        }
    }

    bool isIdle() {
        return Stepper::isIdle();
    }

    bool synchronize() {
        while (!Stepper::isIdle()) {
            if (!waitTask()) {
                reportInterrupted();
                return false;
            }
        }
        return true;
    }

    uint32_t lastCompletedCommand(uint32_t current) {
        return Stepper::isIdle() ? current : Stepper::lastCompletedCommand();
    }

    void abort() {
        if (Stepper::isIdle()) return;
        abortRequested = true;
        Stepper::hold();
    }

    void emergencyStop() {
        Stepper::quickStop();
        setEnabled(false);
    }

    bool queueExtruder(int32_t steps, float feedrate, uint32_t commandNumber) {
        if (StateMachine::getState() == MachineState::Error) {
            CompactResponse::setResult(CompactResponse::BLOCKED_MOTION_ERROR);
            return false;
        }
        settle();
        if (steps == 0) return true;
        const int32_t delta[AX_COUNT] = {0, 0, 0, steps};
        return enqueue(delta, feedrate, commandNumber);
    }

    bool isPositionReferenced() {
        return referenced;
    }

    int32_t extruderSteps() {
        return Stepper::position(AX_E);
    }

    void resetExtruderSteps() {
        Stepper::setPosition(AX_E, 0);
        Planner::setPosition(AX_E, 0);
        eRemainder = 0.0f;
    }

    position::Position getPosition() {
        position::Position p;
        p.x = Stepper::position(AX_X);
        p.y = Stepper::position(AX_Y);
        p.z = Stepper::position(AX_Z);
        return p;
    }

    void handle(uint8_t code, const char *params, uint32_t commandNumber) {
        if (StateMachine::getState() == MachineState::Error && code != 0 && code != 114) {
            CompactResponse::begin(PSTR("ERR"));
            CompactResponse::addStr(SafetyManager::getErrorReason());
            CompactResponse::end();
            CompactResponse::setResult(CompactResponse::BLOCKED_MOTION_ERROR);
            return;
        }

        switch (code) {
            case 10:
                handleMove(false, params, commandNumber);
                break;
            case 11:
                handleMove(true, params, commandNumber);
                break;
            case 12: {
                if (!synchronize()) break;
                settle();
                float v;
                for (uint8_t a = 0; a < 3; ++a) {
                    if (ParamParser::get(params, (char) ('X' + a), v)) {
                        const int32_t steps = lround(v * STEPS_PER_MM[a]);
                        Stepper::setPosition(a, steps);
                        Planner::setPosition(a, steps);
                        limitAxes |= (uint8_t) (1 << a);
                    }
                }
                referenced = true;
                break;
            }
            case 99: {
                if (!synchronize()) break;
                settle();
                float feedrate = 800.0f;
                ParamParser::get(params, 'F', feedrate);

                if (strchr(params, 'X')) {
                    diagnoseAxis(AX_X, HomingConfig::HOMING_X_BOUNCE_OFFSET, feedrate);
                } else if (strchr(params, 'Y')) {
                    diagnoseAxis(AX_Y, HomingConfig::HOMING_Y_BOUNCE_OFFSET, feedrate);
                } else if (strchr(params, 'Z')) {
                    diagnoseAxis(AX_Z, HomingConfig::HOMING_Z_BOUNCE_OFFSET, feedrate);
                } else {
                    CompactResponse::reject(PSTR("NO_AXIS"));
                }
                break;
            }
            case 114: {
                // Posizione reale in questo istante: non attende la fine dei movimenti
                const position::Position p = getPosition();
                CompactResponse::begin(PSTR("POS"), true);
                CompactResponse::addFloat((float) p.x / MotionConfig::STEPS_PER_MM_X, 3);
                CompactResponse::addFloat((float) p.y / MotionConfig::STEPS_PER_MM_Y, 3);
                CompactResponse::addFloat((float) p.z / MotionConfig::STEPS_PER_MM_Z, 3);
                CompactResponse::addInt(referenced ? 1 : 0);
                CompactResponse::end();
                break;
            }
            case 0:
                emergencyStop();
                resyncAfterStop();
                CompactResponse::sendData(PSTR("STP"), PSTR("EMG"));
                break;
            default:
                CompactResponse::reject(PSTR("UNK_MCODE"));
                break;
        }
    }

    bool home(uint8_t axes) {
        if (!synchronize()) return false;
        settle();
        if (axes == 0) axes = 0x07;

        bool ok = true;
        if (ok && (axes & 0x01)) {
            ok = homeAxis(AX_X, EndstopConfig::ENDSTOP_ENABLED_X, EndstopController::isTriggeredX,
                          HomingConfig::HOMING_X_FEEDRATE, HomingConfig::HOMING_X_MAX_TRAVEL,
                          HomingConfig::HOMING_X_BOUNCE_OFFSET);
        }
        if (ok && (axes & 0x02)) {
            ok = homeAxis(AX_Y, EndstopConfig::ENDSTOP_ENABLED_Y, EndstopController::isTriggeredY,
                          HomingConfig::HOMING_Y_FEEDRATE, HomingConfig::HOMING_Y_MAX_TRAVEL,
                          HomingConfig::HOMING_Y_BOUNCE_OFFSET);
        }
        if (ok && (axes & 0x04)) {
            ok = homeAxis(AX_Z, EndstopConfig::ENDSTOP_ENABLED_Z, EndstopController::isTriggeredZ,
                          HomingConfig::HOMING_Z_FEEDRATE, HomingConfig::HOMING_Z_MAX_TRAVEL,
                          HomingConfig::HOMING_Z_BOUNCE_OFFSET);
        }

        if (ok) referenced = true;
        return ok;
    }
}
