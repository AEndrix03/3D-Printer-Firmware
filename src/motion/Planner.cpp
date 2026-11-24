#include <Arduino.h>
#include "../include/motion/Planner.hpp"
#include "../include/motion/MotionConfig.hpp"
#include "../include/hal/McuHAL.hpp"

namespace {
    using namespace Planner;

    constexpr uint8_t MASK = BLOCK_BUFFER_SIZE - 1;
    constexpr uint32_t SATURATED = 0xFFFFFFFFUL;

    // L'accelerazione usata per calcolare le distanze di frenata e' il 90% di quella che lo
    // Stepper applica davvero: la rampa, integrata a passi discreti, resta un po' indietro
    // rispetto alla cinematica ideale e questo margine la fa arrivare in tempo.
    //   velocita'^2 guadagnata per step = 2 * 0.9 * a * 64^2 / (scala / 16) = a * 117965 / scala
    constexpr uint32_t GAIN_PER_STEP = 117965UL;
    constexpr uint16_t MAX_ACCELERATION = 30000;       // mm/s^2: oltre, GAIN_PER_STEP * a non sta in 32 bit
    constexpr uint32_t MAX_ACCEL_STEPS = 4000000UL;    // step/s^2

    // accelK = step/s^2 * 65536 / tick al secondo, come (x * ACCEL_K_MUL) >> 14
    constexpr uint32_t ACCEL_K_MUL = (uint32_t) (65536.0 * 16384.0 / hal::stepTimer::TICKS_PER_SECOND + 0.5);

    Block buffer[BLOCK_BUFFER_SIZE];
    volatile uint8_t head = 0; // prossimo blocco da scrivere (loop principale)
    volatile uint8_t tail = 0; // blocco in esecuzione / prossimo da eseguire (interrupt)

    int32_t plannedPosition[AX_COUNT] = {0, 0, 0, 0};

    inline uint8_t nextIndex(uint8_t i) { return (uint8_t) ((i + 1) & MASK); }

    inline uint8_t prevIndex(uint8_t i) { return (uint8_t) ((i - 1) & MASK); }

    /** Radice quadrata intera (per difetto). */
    uint16_t isqrt(uint32_t value) {
        uint32_t root = 0;
        uint32_t bit = 1UL << 30;
        while (bit > value) bit >>= 2;
        while (bit) {
            if (value >= root + bit) {
                value -= root + bit;
                root = (root >> 1) + bit;
            } else {
                root >>= 1;
            }
            bit >>= 2;
        }
        return (uint16_t) root;
    }

    /** Velocita' (al quadrato, unita' comuni) -> step/s del blocco, entro i suoi limiti. */
    uint16_t rateFor(const Block &b, uint32_t speedSqr) {
        uint32_t rate = ((uint32_t) isqrt(speedSqr) * b.stepScale) >> 10;
        if (rate < b.standstillRate) rate = b.standstillRate;
        if (rate > b.nominalRate) rate = b.nominalRate;
        return (uint16_t) rate;
    }

    /**
     * Traduce velocita' di ingresso/uscita nel profilo trapezoidale usato dallo Stepper.
     * Non tocca i blocchi gia' in esecuzione.
     */
    void calculateTrapezoid(Block &b, uint32_t entrySqr, uint32_t exitSqr) {
        const uint16_t initialRate = rateFor(b, entrySqr);
        const uint16_t finalRate = rateFor(b, exitSqr);

        const uint32_t nominalSq = (uint32_t) b.nominalRate * b.nominalRate;
        const uint32_t initialSq = (uint32_t) initialRate * initialRate;
        const uint32_t finalSq = (uint32_t) finalRate * finalRate;
        const uint32_t count = b.stepEventCount;

        uint32_t accelSteps = (nominalSq - initialSq + b.accel2 - 1) / b.accel2; // per eccesso
        const uint32_t decelSteps = (nominalSq - finalSq) / b.accel2;            // per difetto
        uint32_t decelerateAfter;
        if (accelSteps + decelSteps > count) {
            // Il blocco e' troppo corto per raggiungere la velocita' nominale:
            // si accelera fino al punto di intersezione e poi si decelera.
            const uint32_t gain = b.accel2 * count + finalSq;
            accelSteps = gain > initialSq ? (gain - initialSq + 2 * b.accel2 - 1) / (2 * b.accel2) : 0;
            if (accelSteps > count) accelSteps = count;
            decelerateAfter = accelSteps;
        } else {
            decelerateAfter = count - decelSteps;
        }

        const uint8_t irq = hal::irqSave();
        if (!(b.flags & FLAG_BUSY)) {
            b.initialRate = initialRate;
            b.finalRate = finalRate;
            b.decelerateAfter = (uint16_t) decelerateAfter;
        }
        hal::irqRestore(irq);
    }

    /**
     * Dopo l'aggiunta di un blocco: quelli prima di lui possono entrare piu' veloci, perche'
     * ora c'e' piu' strada per frenare. Si risale la coda finche' qualcosa cambia.
     */
    void recalculate() {
        const uint8_t first = tail; // puo' avanzare durante il calcolo: i blocchi BUSY sono protetti
        if (first == head) return;

        uint8_t i = prevIndex(head);
        uint32_t nextEntry = buffer[i].entrySqr;
        while (i != first) {
            i = prevIndex(i);
            Block &b = buffer[i];
            if (b.flags & FLAG_BUSY) break; // in esecuzione: la sua velocita' di ingresso e' ormai un fatto

            uint32_t reachable = nextEntry + b.deltaSqr;
            if (reachable < nextEntry) reachable = SATURATED;
            const uint32_t entry = b.maxEntrySqr < reachable ? b.maxEntrySqr : reachable;
            // L'uscita di questo blocco e' cambiata: il profilo va rifatto anche se l'ingresso resta uguale
            b.flags |= FLAG_RECALCULATE;
            if (entry == b.entrySqr) break; // i blocchi precedenti dipendono solo da questo valore
            b.entrySqr = entry;
            nextEntry = entry;
        }

        i = first;
        while (i != head) {
            Block &current = buffer[i];
            const uint8_t next = nextIndex(i);
            if (current.flags & FLAG_RECALCULATE) {
                current.flags &= (uint8_t) ~FLAG_RECALCULATE;
                calculateTrapezoid(current, current.entrySqr, next == head ? 0 : buffer[next].entrySqr);
            }
            i = next;
        }
    }
}

namespace Planner {
    void init() {
        const uint8_t irq = hal::irqSave();
        head = tail = 0;
        hal::irqRestore(irq);
    }

    bool isFull() {
        return nextIndex(head) == tail;
    }

    bool isEmpty() {
        return head == tail;
    }

    uint8_t queued() {
        return (uint8_t) ((head - tail) & MASK);
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

    bool addBlock(const int32_t *delta, const Plan &plan, uint32_t commandNumber, uint8_t flags) {
        Block &b = buffer[head];

        uint16_t count = 0;
        b.directionBits = 0;
        for (uint8_t a = 0; a < AX_COUNT; ++a) {
            const uint16_t steps = (uint16_t) labs(delta[a]);
            b.steps[a] = steps;
            if (delta[a] < 0) b.directionBits |= (uint8_t) (1 << a);
            if (steps > count) count = steps;
        }
        if (count == 0) return false;
        b.stepEventCount = count;
        b.stepScale = plan.stepScale ? plan.stepScale : 1;
        b.commandNumber = commandNumber;

        // --- velocita' nominale, entro i limiti del generatore di step ---
        uint32_t rate = ((uint32_t) plan.nominalSpeed * b.stepScale) >> 10;
        if (rate < MotionConfig::MIN_STEP_RATE) rate = MotionConfig::MIN_STEP_RATE;
        if (rate > MotionConfig::MAX_STEP_RATE) rate = MotionConfig::MAX_STEP_RATE;
        b.nominalRate = (uint16_t) rate;

        // --- accelerazione: quella vera per lo Stepper, il 90% per le distanze ---
        uint16_t acceleration = plan.acceleration ? plan.acceleration : 1;
        if (acceleration > MAX_ACCELERATION) acceleration = MAX_ACCELERATION;
        uint32_t accelSteps = ((uint32_t) acceleration * b.stepScale) >> 4; // step/s^2
        if (accelSteps > MAX_ACCEL_STEPS) accelSteps = MAX_ACCEL_STEPS;
        if (accelSteps < 16) accelSteps = 16;
        b.accelK = (accelSteps * ACCEL_K_MUL) >> 14;
        if (b.accelK == 0) b.accelK = 1;
        b.accel2 = (accelSteps * 461UL) >> 8; // 2 * 0.9
        // Da fermo il primo step arriva dopo sqrt(2/a): partire piu' lenti di cosi'
        // significherebbe solo aspettare, non accelerare piu' dolcemente.
        b.standstillRate = isqrt(b.accel2);
        if (b.standstillRate < MotionConfig::MIN_STEP_RATE) b.standstillRate = MotionConfig::MIN_STEP_RATE;

        // --- quanto puo' cambiare la velocita' lungo il blocco ---
        const uint32_t nominalSqr = (uint32_t) plan.nominalSpeed * plan.nominalSpeed;
        uint32_t gain = (uint32_t) acceleration * GAIN_PER_STEP / b.stepScale;
        if (gain == 0) gain = 1;
        // oltre la velocita' nominale non serve saperlo (e il prodotto non starebbe in 32 bit)
        b.deltaSqr = count >= nominalSqr / gain ? SATURATED : gain * count;

        // --- ingresso: quello chiesto dall'host, per ora con arresto a fine blocco ---
        uint32_t entrySqr = (uint32_t) plan.entrySpeed * plan.entrySpeed;
        if (entrySqr > nominalSqr) entrySqr = nominalSqr;
        b.maxEntrySqr = entrySqr;
        b.entrySqr = entrySqr < b.deltaSqr ? entrySqr : b.deltaSqr;
        b.flags = (uint8_t) (flags & FLAG_SEEK_ENDSTOP);

        calculateTrapezoid(b, b.entrySqr, 0);

        for (uint8_t a = 0; a < AX_COUNT; ++a) plannedPosition[a] += delta[a];

        head = nextIndex(head); // da qui lo Stepper puo' vederlo
        recalculate();
        return true;
    }
}
