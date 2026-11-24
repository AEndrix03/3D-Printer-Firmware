#include <Arduino.h>
#include "../include/controllers/MotionController.hpp"
#include "../include/controllers/EndstopController.hpp"
#include "../include/controllers/ExtruderController.hpp"
#include "../include/StateMachine.hpp"
#include "../include/Pins.hpp"
#include "../include/SafetyManager.hpp"
#include "../include/motion/MotionConfig.hpp"
#include "../include/motion/HomingConfig.hpp"
#include "../include/extruder/ExtruderConfig.hpp"
#include "../include/motion/Planner.hpp"
#include "../include/motion/Stepper.hpp"
#include "../include/endstop/EndstopConfig.hpp"
#include "../include/hal/drivers/StepperConfig.hpp"
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

    // Velocita' nominale e accelerazione dell'ultimo movimento ricevuto: l'host le invia
    // solo quando cambiano. 0 = non ancora note.
    uint16_t streamSpeed = 0;
    uint16_t streamAcceleration = 0;
    bool abortRequested = false;
    bool streamLatched = false;   // movimenti binari scartati fino al prossimo comando di testo
    bool referenced = false;
    bool motorsOn = false;
    uint8_t limitAxes = 0;        // bit per asse: posizione riferita, i limiti di corsa valgono

    /** Step per millimetro dell'asse (costanti di compilazione: nessuna tabella in RAM). */
    inline float stepsPerMm(uint8_t axis) {
        if (axis == AX_X) return MotionConfig::STEPS_PER_MM_X;
        if (axis == AX_Y) return MotionConfig::STEPS_PER_MM_Y;
        return MotionConfig::STEPS_PER_MM_Z;
    }

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
    }

    /**
     * Lavori periodici mentre un comando attende (posto in coda o fine dei movimenti):
     * watchdog, BUSY, protezioni e abort dall'host. I byte in arrivo restano nel buffer
     * di ricezione: l'host puo' aver gia' inviato i movimenti successivi.
     * @return false se l'attesa va interrotta
     */
    bool waitTask() {
        hal::watchdog::reset();
        Planner::prepare();
        BusyHandler::update();
        SafetyManager::update();
        MotionController::pollAbort();
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

    /** Accoda un movimento gia' pianificato, aspettando un posto libero se serve. */
    bool enqueue(const int32_t *delta, const Planner::Plan &plan, uint32_t commandNumber, uint8_t flags = 0) {
        while (Planner::isFull()) {
            if (!waitTask()) {
                reportInterrupted();
                return false;
            }
        }
        // I motori spenti (emergenza rientrata, risparmio energetico) si riaccendono al primo movimento
        if (!motorsOn) MotionController::setEnabled(true);
        Planner::addBlock(delta, plan, commandNumber, flags);
        return true;
    }

    /**
     * Movimento di un solo asse deciso dal firmware (homing, diagnosi, comandi dell'estrusore):
     * parte e arriva da fermo, entro i limiti dell'asse. Le corse lunghe diventano piu' blocchi.
     */
    bool moveAxis(uint8_t axis, int32_t steps, float feedrate, uint32_t commandNumber, uint8_t flags = 0) {
        float spm, maxFeed, acceleration;
        if (axis == AX_E) {
            spm = ExtruderController::getStepsPerMm();
            maxFeed = ExtruderConfig::MAX_FEEDRATE_E;
            acceleration = MotionConfig::ACCELERATION_E;
        } else if (axis == AX_Z) {
            spm = MotionConfig::STEPS_PER_MM_Z;
            maxFeed = MotionConfig::MAX_FEEDRATE_Z;
            acceleration = MotionConfig::ACCELERATION_Z;
        } else {
            spm = stepsPerMm(axis);
            maxFeed = MotionConfig::MAX_FEEDRATE_X;
            acceleration = MotionConfig::ACCELERATION_XY;
        }
        if (feedrate > maxFeed) feedrate = maxFeed;
        if (feedrate < MotionConfig::MIN_FEEDRATE) feedrate = MotionConfig::MIN_FEEDRATE;

        Planner::Plan plan;
        plan.nominalSpeed = (uint16_t) (feedrate * (64.0f / 60.0f));
        plan.acceleration = (uint16_t) acceleration;
        plan.stepScale = (uint16_t) (spm * 16.0f);
        plan.entrySpeed = 0;

        int32_t delta[AX_COUNT] = {0, 0, 0, 0};
        while (steps != 0) {
            int32_t part = steps;
            if (part > Planner::MAX_BLOCK_STEPS) part = Planner::MAX_BLOCK_STEPS;
            if (part < -Planner::MAX_BLOCK_STEPS) part = -Planner::MAX_BLOCK_STEPS;
            delta[axis] = part;
            if (!enqueue(delta, plan, commandNumber, flags)) return false;
            steps -= part;
            plan.entrySpeed = plan.nominalSpeed; // i blocchi successivi proseguono sulla stessa retta
        }
        return true;
    }

    /**
     * Muove un asse fino al finecorsa (o per maxSteps) e attende la fine.
     * @return step realmente percorsi, -1 se interrotto
     */
    int32_t seek(uint8_t axis, bool positive, int32_t maxSteps, float feedrate, bool untilEndstop,
                 uint32_t commandNumber) {
        const int32_t before = Stepper::position(axis);

        if (!moveAxis(axis, positive ? maxSteps : -maxSteps, feedrate, commandNumber,
                      untilEndstop ? Planner::FLAG_SEEK_ENDSTOP : 0)) return -1;
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
            if (seek(axis, false, (int32_t) (maxTravelMm * stepsPerMm(axis)), feedrate, true, 0) < 0) return false;
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
        // Step e direzione: uscite. Il verso (STEPPER_INVERT_*) lo applica lo Stepper.
        pinMode(PIN_X_STEP, OUTPUT);
        pinMode(PIN_X_DIR, OUTPUT);
        pinMode(PIN_Y_STEP, OUTPUT);
        pinMode(PIN_Y_DIR, OUTPUT);
        pinMode(PIN_Z_STEP, OUTPUT);
        pinMode(PIN_Z_DIR, OUTPUT);

        abortRequested = false;
        referenced = false;
        limitAxes = 0;
        pinMode(PIN_STEPPERS_ENABLE, OUTPUT);
        setEnabled(true);
        streamSpeed = 0;
        streamAcceleration = 0;
        Planner::init();
        Stepper::init();
    }

    void update() {
        resyncAfterStop();
        Planner::prepare();
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
        // I movimenti binari gia' in volo non devono ripartire dopo l'arresto
        streamLatched = true;
        if (Stepper::isIdle()) return;
        abortRequested = true;
        Stepper::hold();
    }

    void pollAbort() {
        if (hal::serial::takeAbortRequest()) abort();
    }

    void releaseStream() {
        streamLatched = false;
    }

    void handleStreamMove(const int32_t *delta, Planner::Plan plan, uint32_t commandNumber) {
        if (StateMachine::getState() == MachineState::Error) {
            CompactResponse::begin(PSTR("ERR"));
            CompactResponse::addStr(SafetyManager::getErrorReason());
            CompactResponse::end();
            CompactResponse::setResult(CompactResponse::BLOCKED_MOTION_ERROR);
            return;
        }
        if (streamLatched) {
            CompactResponse::reject(PSTR("SKIPPED"));
            return;
        }

        if (plan.nominalSpeed) {
            streamSpeed = plan.nominalSpeed;
            streamAcceleration = plan.acceleration;
        } else {
            plan.nominalSpeed = streamSpeed;
            plan.acceleration = streamAcceleration;
        }

        PGM_P refusal = nullptr;
        if (plan.nominalSpeed == 0 || plan.acceleration == 0 || plan.stepScale == 0) {
            refusal = PSTR("NO_PLAN"); // velocita' mai ricevute (firmware riavviato): l'host deve riallinearsi
        } else if (labs(delta[AX_X]) > Planner::MAX_BLOCK_STEPS || labs(delta[AX_Y]) > Planner::MAX_BLOCK_STEPS ||
                   labs(delta[AX_Z]) > Planner::MAX_BLOCK_STEPS || labs(delta[AX_E]) > Planner::MAX_BLOCK_STEPS) {
            refusal = PSTR("RANGE");
        } else if (StateMachine::getState() != MachineState::Printing) {
            refusal = PSTR("INV_STATE");
        } else {
            settle();
            if (!withinTravel(delta)) refusal = PSTR("LIMIT");
            else if (delta[AX_E] != 0 && !ExtruderController::canExtrude()) refusal = PSTR("COLD_EXT");
        }
        if (refusal) {
            streamLatched = true;
            CompactResponse::reject(refusal);
            return;
        }

        if (delta[AX_X] || delta[AX_Y] || delta[AX_Z] || delta[AX_E]) {
            if (!enqueue(delta, plan, commandNumber)) streamLatched = true; // interrotto in attesa
        }
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
        return moveAxis(AX_E, steps, feedrate, commandNumber);
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
    }

    position::Position getPosition() {
        position::Position p;
        p.x = Stepper::position(AX_X);
        p.y = Stepper::position(AX_Y);
        p.z = Stepper::position(AX_Z);
        return p;
    }

    void handle(uint8_t code, const char *params, uint32_t /*commandNumber*/) {
        if (StateMachine::getState() == MachineState::Error && code != 0 && code != 114 && code != 115) {
            CompactResponse::begin(PSTR("ERR"));
            CompactResponse::addStr(SafetyManager::getErrorReason());
            CompactResponse::end();
            CompactResponse::setResult(CompactResponse::BLOCKED_MOTION_ERROR);
            return;
        }

        switch (code) {
            case 12: {
                if (!synchronize()) break;
                settle();
                float v;
                for (uint8_t a = 0; a < 3; ++a) {
                    if (ParamParser::get(params, (char) ('X' + a), v)) {
                        const int32_t steps = lround(v * stepsPerMm(a));
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
            case 115: {
                // Per l'host che invia i movimenti in step: fattori di conversione e posizione
                // del planner (fine dei movimenti in coda), cioe' il punto da cui parte il prossimo.
                CompactResponse::begin(PSTR("SPM"));
                CompactResponse::addFloat(MotionConfig::STEPS_PER_MM_X, 2);
                CompactResponse::addFloat(MotionConfig::STEPS_PER_MM_Y, 2);
                CompactResponse::addFloat(MotionConfig::STEPS_PER_MM_Z, 2);
                CompactResponse::addFloat(ExtruderController::getStepsPerMm(), 2);
                CompactResponse::end();
                // Limiti della macchina: l'host pianifica con questi, cosi' restano scritti in un posto solo
                CompactResponse::begin(PSTR("LIM"));
                CompactResponse::addInt((int32_t) MotionConfig::MAX_FEEDRATE_X);
                CompactResponse::addInt((int32_t) MotionConfig::MAX_FEEDRATE_Y);
                CompactResponse::addInt((int32_t) MotionConfig::MAX_FEEDRATE_Z);
                CompactResponse::addInt((int32_t) ExtruderConfig::MAX_FEEDRATE_E);
                CompactResponse::addInt((int32_t) MotionConfig::ACCELERATION_XY);
                CompactResponse::addInt((int32_t) MotionConfig::ACCELERATION_Z);
                CompactResponse::addInt((int32_t) MotionConfig::ACCELERATION_E);
                CompactResponse::addInt((int32_t) (MotionConfig::JUNCTION_DEVIATION * 1000.0f)); // micron
                CompactResponse::addInt(MotionConfig::MAX_STEP_RATE);
                CompactResponse::addInt(Planner::BLOCK_BUFFER_SIZE - 1);
                CompactResponse::addInt(hal::serial::RX_BUFFER_SIZE);
                CompactResponse::end();
                // Corsa massima in step e assi su cui vale: l'host rifiuta da se' un movimento
                // fuori volume prima di spezzarlo in piu' blocchi (qui il controllo resta comunque)
                CompactResponse::begin(PSTR("TRV"));
                for (uint8_t a = 0; a < 3; ++a) {
                    CompactResponse::addInt((int32_t) pgm_read_dword(&TRAVEL_MAX_STEPS[a]));
                }
                CompactResponse::addInt(MotionConfig::SOFT_LIMITS_ENABLED ? limitAxes : 0);
                CompactResponse::end();
                settle();
                CompactResponse::begin(PSTR("STP"), true);
                CompactResponse::addInt(Planner::position(AX_X));
                CompactResponse::addInt(Planner::position(AX_Y));
                CompactResponse::addInt(Planner::position(AX_Z));
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
