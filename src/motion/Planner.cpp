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

    // Valori che dipendono solo da accelerazione e scala: uguali per tutti i segmenti di una
    // stessa retta, quindi si ricalcolano (due divisioni e una radice) solo quando cambiano.
    uint16_t cachedAcceleration = 0;
    uint16_t cachedScale = 0;
    uint32_t cachedGain = 1;
    uint32_t cachedInverse = SATURATED;
    uint32_t cachedAccelK = 1;
    uint16_t cachedStandstill = MotionConfig::MIN_STEP_RATE;

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

    /**
     * Step necessari per cambiare la velocita'^2 di `speedSqr`: speedSqr / guadagno per step,
     * calcolato come prodotto per l'inverso (su AVR una divisione a 32 bit costa ~40 us).
     * Il risultato e' quello esatto o uno in piu': mai meno strada del necessario.
     */
    inline uint32_t stepsFor(const Block &b, uint32_t speedSqr) {
        return (uint32_t) (((uint64_t) speedSqr * b.gainInverse) >> 32) + 1;
    }

    /**
     * Traduce velocita' di ingresso/uscita (al quadrato, unita' comuni) nel profilo usato
     * dallo Stepper: da quale step frenare e fino a che velocita'. Tutto in step, senza
     * passare dalle velocita' in step/s, tranne quella di arrivo.
     * Non tocca i blocchi gia' in esecuzione.
     */
    void calculateTrapezoid(Block &b, uint32_t entrySqr, uint32_t exitSqr) {
        const uint32_t nominalSqr = (uint32_t) b.nominalSpeed * b.nominalSpeed;
        if (entrySqr > nominalSqr) entrySqr = nominalSqr;
        if (exitSqr > nominalSqr) exitSqr = nominalSqr;
        const uint32_t count = b.stepEventCount;

        const uint32_t accelSteps = stepsFor(b, nominalSqr - entrySqr);
        const uint32_t decelSteps = stepsFor(b, nominalSqr - exitSqr);
        uint32_t decelerateAfter;
        if (accelSteps + decelSteps > count) {
            // Il blocco e' troppo corto per raggiungere la velocita' nominale: si accelera fino
            // al punto da cui la frenata porta giusta alla velocita' di uscita.
            if (exitSqr >= entrySqr) {
                decelerateAfter = (count + stepsFor(b, exitSqr - entrySqr)) >> 1;
                if (decelerateAfter > count) decelerateAfter = count;
            } else {
                const uint32_t braking = stepsFor(b, entrySqr - exitSqr);
                decelerateAfter = count > braking ? (count - braking) >> 1 : 0;
            }
        } else {
            decelerateAfter = count - decelSteps;
        }

        uint32_t finalRate = ((uint32_t) isqrt(exitSqr) * b.stepScale) >> 10;
        if (finalRate < b.standstillRate) finalRate = b.standstillRate;
        if (finalRate > b.nominalRate) finalRate = b.nominalRate;

        const uint8_t irq = hal::irqSave();
        if (!(b.flags & FLAG_BUSY)) {
            b.finalRate = (uint16_t) finalRate;
            b.decelerateAfter = (uint16_t) decelerateAfter;
        }
        hal::irqRestore(irq);
    }

    /**
     * Dopo l'aggiunta di un blocco: quelli prima di lui possono entrare piu' veloci, perche'
     * ora c'e' piu' strada per frenare. Si risale la coda finche' qualcosa cambia.
     *
     * Qui si aggiornano solo le velocita' di ingresso (somme e confronti). I profili, che
     * costano radici e divisioni, li rifa' prepare() e solo per i blocchi che stanno per partire.
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
        if (acceleration != cachedAcceleration || b.stepScale != cachedScale) {
            cachedAcceleration = acceleration;
            cachedScale = b.stepScale;
            uint32_t accelSteps = ((uint32_t) acceleration * b.stepScale) >> 4; // step/s^2
            if (accelSteps > MAX_ACCEL_STEPS) accelSteps = MAX_ACCEL_STEPS;
            if (accelSteps < 16) accelSteps = 16;
            cachedAccelK = (accelSteps * ACCEL_K_MUL) >> 14;
            if (cachedAccelK == 0) cachedAccelK = 1;
            // Da fermo il primo step arriva dopo sqrt(2/a): partire piu' lenti di cosi'
            // significherebbe solo aspettare, non accelerare piu' dolcemente.
            cachedStandstill = isqrt((accelSteps * 461UL) >> 8); // sqrt(2 * 0.9 * a)
            if (cachedStandstill < MotionConfig::MIN_STEP_RATE) cachedStandstill = MotionConfig::MIN_STEP_RATE;
            cachedGain = (uint32_t) acceleration * GAIN_PER_STEP / b.stepScale;
            if (cachedGain == 0) cachedGain = 1;
            cachedInverse = SATURATED / cachedGain;
        }
        b.accelK = cachedAccelK;
        b.standstillRate = cachedStandstill;
        b.gainInverse = cachedInverse;
        b.nominalSpeed = plan.nominalSpeed ? plan.nominalSpeed : 1;

        // --- quanto puo' cambiare la velocita' lungo il blocco ---
        const uint32_t nominalSqr = (uint32_t) b.nominalSpeed * b.nominalSpeed;
        // oltre la velocita' nominale non serve saperlo (e il prodotto non starebbe in 32 bit)
        b.deltaSqr = count >= stepsFor(b, nominalSqr) ? SATURATED : cachedGain * count;

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
        prepare();
        return true;
    }

    void prepare() {
        uint8_t i = tail;
        for (uint8_t n = 0; n < PREPARED_BLOCKS && i != head; ++n) {
            Block &b = buffer[i];
            const uint8_t next = nextIndex(i);
            if ((b.flags & (FLAG_RECALCULATE | FLAG_BUSY)) == FLAG_RECALCULATE) {
                b.flags &= (uint8_t) ~FLAG_RECALCULATE;
                calculateTrapezoid(b, b.entrySqr, next == head ? 0 : buffer[next].entrySqr);
            }
            i = next;
        }
    }
}
