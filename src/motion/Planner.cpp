#include <Arduino.h>
#include "../include/motion/Planner.hpp"
#include "../include/motion/MotionConfig.hpp"
#include "../include/extruder/ExtruderConfig.hpp"
#include "../include/controllers/ExtruderController.hpp"
#include "../include/hal/McuHAL.hpp"

namespace {
    using namespace Planner;

    constexpr uint8_t MASK = BLOCK_BUFFER_SIZE - 1;
    constexpr float PLANNING_MARGIN = 0.9f;
    constexpr float MIN_PLANNER_SPEED = 0.05f; // mm/s: velocita' di partenza/arrivo "da fermo"

    Block buffer[BLOCK_BUFFER_SIZE];
    volatile uint8_t head = 0; // prossimo blocco da scrivere (loop principale)
    volatile uint8_t tail = 0; // blocco in esecuzione / prossimo da eseguire (interrupt)

    int32_t plannedPosition[AX_COUNT] = {0, 0, 0, 0};
    float previousUnit[3] = {0.0f, 0.0f, 0.0f};
    float previousNominalSpeed = 0.0f;
    float previousAcceleration = 0.0f;
    bool previousHadXYZ = false;

    inline uint8_t nextIndex(uint8_t i) { return (uint8_t) ((i + 1) & MASK); }

    inline uint8_t prevIndex(uint8_t i) { return (uint8_t) ((i - 1) & MASK); }

    /** Velocita' massima da cui si riesce ad arrivare a `target` in `distance` con accelerazione `accel`. */
    inline float maxAllowableSpeed(float accel, float target, float distance) {
        return sqrt(target * target + 2.0f * accel * distance);
    }

    /**
     * Traduce velocita' di ingresso/uscita (mm/s) nel profilo a interi usato dallo Stepper.
     * Non tocca i blocchi gia' in esecuzione.
     */
    void calculateTrapezoid(Block &b, float entrySpeed, float exitSpeed) {
        const float ratePerSpeed = (float) b.nominalRate / b.nominalSpeed;

        const float accelSteps = b.acceleration * ((float) b.stepEventCount / b.millimeters); // step/s^2

        // Da fermo il primo step arriva dopo sqrt(2/a): partire piu' lenti di cosi'
        // significherebbe solo aspettare, non accelerare piu' dolcemente.
        uint32_t standstillRate = (uint32_t) sqrt(2.0f * accelSteps);
        if (standstillRate < MotionConfig::MIN_STEP_RATE) standstillRate = MotionConfig::MIN_STEP_RATE;

        uint32_t initialRate = (uint32_t) ceil(entrySpeed * ratePerSpeed);
        uint32_t finalRate = (uint32_t) ceil(exitSpeed * ratePerSpeed);
        if (initialRate < standstillRate) initialRate = standstillRate;
        if (finalRate < standstillRate) finalRate = standstillRate;
        if (initialRate > b.nominalRate) initialRate = b.nominalRate;
        if (finalRate > b.nominalRate) finalRate = b.nominalRate;
        const float nominalSqr = (float) b.nominalRate * (float) b.nominalRate;
        const float initialSqr = (float) initialRate * (float) initialRate;
        const float finalSqr = (float) finalRate * (float) finalRate;

        float accelDistance = ceil((nominalSqr - initialSqr) / (2.0f * accelSteps));
        const float decelDistance = floor((nominalSqr - finalSqr) / (2.0f * accelSteps));
        const float count = (float) b.stepEventCount;

        float decelerateAfter = count - decelDistance;
        if (accelDistance + decelDistance > count) {
            // Il blocco e' troppo corto per raggiungere la velocita' nominale:
            // si accelera fino al punto di intersezione e poi si decelera.
            accelDistance = ceil((2.0f * accelSteps * count - initialSqr + finalSqr) / (4.0f * accelSteps));
            if (accelDistance < 0.0f) accelDistance = 0.0f;
            if (accelDistance > count) accelDistance = count;
            decelerateAfter = accelDistance;
        }

        const uint8_t irq = hal::irqSave();
        if (!(b.flags & FLAG_BUSY)) {
            b.initialRate = (uint16_t) initialRate;
            b.finalRate = (uint16_t) finalRate;
            b.decelerateAfter = (uint32_t) decelerateAfter;
        }
        hal::irqRestore(irq);
    }

    /**
     * Ricalcola le velocita' di giunzione di tutta la coda.
     * Passata all'indietro: ogni blocco entra al massimo alla velocita' da cui riesce
     * ancora a frenare fino all'ingresso del successivo. Passata in avanti: nessun blocco
     * entra piu' veloce di quanto il precedente riesca ad accelerare.
     */
    void recalculate() {
        const uint8_t first = tail; // puo' avanzare durante il calcolo: i blocchi BUSY sono protetti
        if (first == head) return;

        // --- all'indietro ---
        float nextEntry = MIN_PLANNER_SPEED;
        uint8_t i = head;
        while (i != first) {
            i = prevIndex(i);
            Block &b = buffer[i];
            if (b.flags & FLAG_BUSY) {
                // in esecuzione: la sua velocita' di ingresso e' ormai un fatto
                nextEntry = b.entrySpeed;
                continue;
            }
            if (b.entrySpeed != b.maxEntrySpeed) {
                if (!(b.flags & FLAG_NOMINAL_LENGTH) && b.maxEntrySpeed > nextEntry) {
                    const float reachable = maxAllowableSpeed(b.acceleration, nextEntry, b.millimeters);
                    b.entrySpeed = reachable < b.maxEntrySpeed ? reachable : b.maxEntrySpeed;
                } else {
                    b.entrySpeed = b.maxEntrySpeed;
                }
                b.flags |= FLAG_RECALCULATE;
            }
            nextEntry = b.entrySpeed;
        }

        // --- in avanti ---
        i = first;
        uint8_t next = nextIndex(i);
        while (next != head) {
            Block &current = buffer[i];
            Block &following = buffer[next];
            if (!(current.flags & FLAG_NOMINAL_LENGTH) && current.entrySpeed < following.entrySpeed) {
                const float reachable = maxAllowableSpeed(current.acceleration, current.entrySpeed,
                                                          current.millimeters);
                if (reachable < following.entrySpeed) {
                    following.entrySpeed = reachable;
                    following.flags |= FLAG_RECALCULATE;
                }
            }
            i = next;
            next = nextIndex(next);
        }

        // --- profili ---
        i = first;
        while (i != head) {
            Block &current = buffer[i];
            next = nextIndex(i);
            const bool last = (next == head);
            if ((current.flags & FLAG_RECALCULATE) || (!last && (buffer[next].flags & FLAG_RECALCULATE))) {
                calculateTrapezoid(current, current.entrySpeed, last ? MIN_PLANNER_SPEED : buffer[next].entrySpeed);
            }
            i = next;
        }
        i = first;
        while (i != head) {
            buffer[i].flags &= ~FLAG_RECALCULATE;
            i = nextIndex(i);
        }
    }
}

namespace Planner {
    void init() {
        const uint8_t irq = hal::irqSave();
        head = tail = 0;
        hal::irqRestore(irq);
        previousNominalSpeed = 0.0f;
        previousHadXYZ = false;
    }

    bool isFull() {
        return nextIndex(head) == tail;
    }

    bool isEmpty() {
        return head == tail;
    }

    int32_t position(uint8_t axis) {
        return plannedPosition[axis];
    }

    void setPosition(uint8_t axis, int32_t steps) {
        plannedPosition[axis] = steps;
    }

    void flush() {
        const uint8_t irq = hal::irqSave();
        head = tail;
        hal::irqRestore(irq);
        previousNominalSpeed = 0.0f;
        previousHadXYZ = false;
    }

    Block *acquireBlock() {
        if (head == tail) return nullptr;
        Block *b = &buffer[tail];
        b->flags |= FLAG_BUSY;
        return b;
    }

    void releaseBlock() {
        if (head != tail) tail = nextIndex(tail);
    }

    bool addLine(const int32_t *delta, float feedrate, uint32_t commandNumber, uint8_t flags) {
        Block &b = buffer[head];

        const float stepsPerMm[AX_COUNT] = {
                MotionConfig::STEPS_PER_MM_X, MotionConfig::STEPS_PER_MM_Y,
                MotionConfig::STEPS_PER_MM_Z, ExtruderController::getStepsPerMm()
        };
        const float maxFeed[AX_COUNT] = { // mm/s
                MotionConfig::MAX_FEEDRATE_X / 60.0f, MotionConfig::MAX_FEEDRATE_Y / 60.0f,
                MotionConfig::MAX_FEEDRATE_Z / 60.0f, ExtruderConfig::MAX_FEEDRATE_E / 60.0f
        };
        const float maxAccel[AX_COUNT] = { // mm/s^2
                MotionConfig::ACCELERATION_XY, MotionConfig::ACCELERATION_XY,
                MotionConfig::ACCELERATION_Z, MotionConfig::ACCELERATION_E
        };

        float mm[AX_COUNT];
        b.stepEventCount = 0;
        b.directionBits = 0;
        for (uint8_t a = 0; a < AX_COUNT; ++a) {
            b.steps[a] = (uint32_t) labs(delta[a]);
            if (delta[a] < 0) b.directionBits |= (uint8_t) (1 << a);
            if (b.steps[a] > b.stepEventCount) b.stepEventCount = b.steps[a];
            mm[a] = (float) delta[a] / stepsPerMm[a];
        }
        if (b.stepEventCount == 0) return false;

        const float cartesian = sqrt(mm[AX_X] * mm[AX_X] + mm[AX_Y] * mm[AX_Y] + mm[AX_Z] * mm[AX_Z]);
        const bool hasXYZ = cartesian > 0.0001f;
        b.millimeters = hasXYZ ? cartesian : fabs(mm[AX_E]);
        const float inverseMm = 1.0f / b.millimeters;

        // --- velocita' nominale: F lungo il percorso, entro i limiti di ogni asse e del generatore di step ---
        float speed = feedrate / 60.0f;
        if (speed < MotionConfig::MIN_FEEDRATE / 60.0f) speed = MotionConfig::MIN_FEEDRATE / 60.0f;
        float acceleration = hasXYZ ? MotionConfig::ACCELERATION_XY : MotionConfig::ACCELERATION_E;
        for (uint8_t a = 0; a < AX_COUNT; ++a) {
            const float share = fabs(mm[a]) * inverseMm; // quota del percorso su questo asse
            if (share <= 0.0f) continue;
            if (speed * share > maxFeed[a]) speed = maxFeed[a] / share;
            if (acceleration * share > maxAccel[a]) acceleration = maxAccel[a] / share;
        }

        const float stepsPerMmPath = (float) b.stepEventCount * inverseMm;
        if (speed * stepsPerMmPath > (float) MotionConfig::MAX_STEP_RATE) {
            speed = (float) MotionConfig::MAX_STEP_RATE / stepsPerMmPath;
        }

        b.nominalSpeed = speed;
        // Il planner calcola distanze e velocita' di giunzione con il 90% dell'accelerazione
        // che lo Stepper applica davvero: la rampa, integrata a passi discreti, resta un po'
        // indietro rispetto alla cinematica ideale e questo margine la fa arrivare in tempo.
        b.acceleration = acceleration * PLANNING_MARGIN;
        uint32_t rate = (uint32_t) ceil(speed * stepsPerMmPath);
        if (rate < MotionConfig::MIN_STEP_RATE) rate = MotionConfig::MIN_STEP_RATE;
        b.nominalRate = (uint16_t) rate;
        // incremento di velocita' (step/s) per tick di timer, in virgola fissa 16.16
        b.accelK = (uint32_t) (acceleration * stepsPerMmPath * (65536.0f / (float) hal::stepTimer::TICKS_PER_SECOND));
        if (b.accelK == 0) b.accelK = 1;
        b.commandNumber = commandNumber;

        // --- velocita' massima alla giunzione con il blocco precedente (junction deviation) ---
        float unit[3] = {0.0f, 0.0f, 0.0f};
        float junctionSpeed = MIN_PLANNER_SPEED;
        if (hasXYZ) {
            for (uint8_t a = 0; a < 3; ++a) unit[a] = mm[a] * inverseMm;

            if (head != tail && previousHadXYZ && !(flags & FLAG_SEEK_ENDSTOP)) {
                const float cosTheta = -(previousUnit[0] * unit[0] + previousUnit[1] * unit[1] +
                                         previousUnit[2] * unit[2]);
                const float limit = speed < previousNominalSpeed ? speed : previousNominalSpeed;

                if (cosTheta < -0.999999f) {
                    junctionSpeed = limit; // stessa direzione: nessun rallentamento
                } else if (cosTheta < 0.999999f) {
                    const float sinHalf = sqrt(0.5f * (1.0f - cosTheta));
                    const float junctionAccel = b.acceleration < previousAcceleration ? b.acceleration
                                                                                       : previousAcceleration;
                    junctionSpeed = sqrt(junctionAccel * MotionConfig::JUNCTION_DEVIATION * sinHalf /
                                         (1.0f - sinHalf));
                    if (junctionSpeed > limit) junctionSpeed = limit;
                    if (junctionSpeed < MIN_PLANNER_SPEED) junctionSpeed = MIN_PLANNER_SPEED;
                } // inversione completa: resta la velocita' minima
            }
        }

        b.maxEntrySpeed = junctionSpeed;
        const float allowable = maxAllowableSpeed(b.acceleration, MIN_PLANNER_SPEED, b.millimeters);
        b.entrySpeed = junctionSpeed < allowable ? junctionSpeed : allowable;
        b.flags = (uint8_t) (FLAG_RECALCULATE | (flags & FLAG_SEEK_ENDSTOP) | (hasXYZ ? FLAG_HAS_XYZ : 0) |
                             (speed <= allowable ? FLAG_NOMINAL_LENGTH : 0));

        calculateTrapezoid(b, b.entrySpeed, MIN_PLANNER_SPEED);

        previousUnit[0] = unit[0];
        previousUnit[1] = unit[1];
        previousUnit[2] = unit[2];
        previousNominalSpeed = speed;
        previousAcceleration = b.acceleration;
        previousHadXYZ = hasXYZ && !(flags & FLAG_SEEK_ENDSTOP);

        for (uint8_t a = 0; a < AX_COUNT; ++a) plannedPosition[a] += delta[a];

        head = nextIndex(head); // da qui lo Stepper puo' vederlo
        recalculate();
        return true;
    }
}
