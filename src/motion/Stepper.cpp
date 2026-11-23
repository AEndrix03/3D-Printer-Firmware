#include <Arduino.h>
#include "../include/motion/Stepper.hpp"
#include "../include/motion/Planner.hpp"
#include "../include/motion/MotionConfig.hpp"
#include "../include/motion/StepTimerTable.hpp"
#include "../include/controllers/EndstopController.hpp"
#include "../include/hal/drivers/StepperConfig.hpp"
#include "../include/hal/FastPin.hpp"
#include "../include/hal/McuHAL.hpp"
#include "../include/Pins.hpp"

namespace {
    using Planner::Block;
    using Planner::AX_COUNT;

    constexpr uint16_t IDLE_TICKS = 2000; // a coda vuota l'interrupt controlla ogni millisecondo

    Block *current = nullptr;
    uint32_t counter[AX_COUNT];
    uint32_t completed = 0;
    uint32_t rateFixed = 0; // velocita' attuale, step/s in virgola fissa 16.16
    uint16_t timerTicks = IDLE_TICKS;

    volatile int32_t positionSteps[AX_COUNT] = {0, 0, 0, 0};
    volatile uint32_t lastCompleted = 0;
    volatile bool holding = false;
    volatile bool stopEvent = false;
    volatile bool endstopFlag = false;
    volatile bool running = false;

    /**
     * Intervallo del timer per una velocita' in step/s, senza divisioni: tabella in
     * flash con interpolazione lineare (vedi tools/gen_step_table.py).
     */
    inline uint16_t ticksForRate(uint16_t rate) {
        if (rate >= 512) {
            if (rate > 32767) rate = 32767;
            const uint16_t *entry = StepTimerTable::FAST[rate >> 7];
            const uint16_t base = pgm_read_word(&entry[0]);
            const uint16_t delta = pgm_read_word(&entry[1]);
            return base - (uint16_t) (((uint32_t) delta * (uint8_t) (rate & 127)) >> 7);
        }
        const uint16_t *entry = StepTimerTable::SLOW[rate >> 3];
        const uint16_t base = pgm_read_word(&entry[0]);
        const uint16_t delta = pgm_read_word(&entry[1]);
        return base - (uint16_t) (((uint32_t) delta * (uint8_t) (rate & 7)) >> 3);
    }

    inline void setDirections(uint8_t bits) {
        // bit = 1 -> verso negativo; l'inversione dipende dal cablaggio dell'asse
        hal::fastWrite(PIN_X_DIR, (bool) (bits & 0x01) == StepperConfig::STEPPER_INVERT_X);
        hal::fastWrite(PIN_Y_DIR, (bool) (bits & 0x02) == StepperConfig::STEPPER_INVERT_Y);
        hal::fastWrite(PIN_Z_DIR, (bool) (bits & 0x04) == StepperConfig::STEPPER_INVERT_Z);
        hal::fastWrite(PIN_E_DIR, (bool) (bits & 0x08) == StepperConfig::STEPPER_INVERT_E);
    }

    inline bool seekEndstopReached(const Block *b) {
        return (b->steps[0] && EndstopController::isTriggeredX()) ||
               (b->steps[1] && EndstopController::isTriggeredY()) ||
               (b->steps[2] && EndstopController::isTriggeredZ());
    }

    /** Ferma tutto e svuota la coda. Solo da interrupt o a interrupt disabilitati. */
    inline void stopNow() {
        current = nullptr;
        Planner::flush();
        holding = false;
        running = false;
        stopEvent = true;
        timerTicks = IDLE_TICKS;
    }
}

namespace Stepper {
    void init() {
        const uint8_t irq = hal::irqSave();
        current = nullptr;
        holding = false;
        stopEvent = false;
        endstopFlag = false;
        running = false;
        timerTicks = IDLE_TICKS;
        hal::irqRestore(irq);

        hal::stepTimer::start(isr);
        hal::stepTimer::setInterval(IDLE_TICKS);
    }

    bool isIdle() {
        return !running && Planner::isEmpty();
    }

    int32_t position(uint8_t axis) {
        const uint8_t irq = hal::irqSave();
        const int32_t value = positionSteps[axis];
        hal::irqRestore(irq);
        return value;
    }

    void setPosition(uint8_t axis, int32_t steps) {
        const uint8_t irq = hal::irqSave();
        positionSteps[axis] = steps;
        hal::irqRestore(irq);
    }

    uint32_t lastCompletedCommand() {
        const uint8_t irq = hal::irqSave();
        const uint32_t value = lastCompleted;
        hal::irqRestore(irq);
        return value;
    }

    void hold() {
        const uint8_t irq = hal::irqSave();
        if (running || !Planner::isEmpty()) holding = true;
        hal::irqRestore(irq);
    }

    bool isHolding() {
        return holding;
    }

    void quickStop() {
        const uint8_t irq = hal::irqSave();
        if (running || !Planner::isEmpty()) stopNow();
        hal::irqRestore(irq);
    }

    bool consumeStopEvent() {
        const uint8_t irq = hal::irqSave();
        const bool value = stopEvent;
        stopEvent = false;
        hal::irqRestore(irq);
        return value;
    }

    bool endstopHit() {
        return endstopFlag;
    }

    void isr() {
        // --- carica il prossimo blocco ---
        if (!current) {
            current = Planner::acquireBlock();
            if (!current) {
                running = false;
                if (holding) stopNow(); // arresto richiesto e coda gia' esaurita
                hal::stepTimer::setInterval(IDLE_TICKS);
                return;
            }

            const uint32_t half = current->stepEventCount >> 1;
            for (uint8_t a = 0; a < AX_COUNT; ++a) counter[a] = half;
            completed = 0;
            setDirections(current->directionBits);

            if (!running) {
                // partenza da fermo: velocita' iniziale pianificata
                rateFixed = (uint32_t) current->initialRate << 16;
                running = true;
            } else if ((uint16_t) (rateFixed >> 16) > current->nominalRate) {
                rateFixed = (uint32_t) current->nominalRate << 16;
            }
            if (current->flags & Planner::FLAG_SEEK_ENDSTOP) endstopFlag = false;
            timerTicks = ticksForRate((uint16_t) (rateFixed >> 16));
        }

        Block *b = current;

        if ((b->flags & Planner::FLAG_SEEK_ENDSTOP) && seekEndstopReached(b)) {
            endstopFlag = true;
            stopNow();
            hal::stepTimer::setInterval(IDLE_TICKS);
            return;
        }

        // --- Bresenham: uno "step event" dell'asse dominante ---
        const uint8_t bits = b->directionBits;
        counter[0] += b->steps[0];
        if (counter[0] >= b->stepEventCount) {
            counter[0] -= b->stepEventCount;
            hal::fastWrite(PIN_X_STEP, true);
            positionSteps[0] += (bits & 0x01) ? -1 : 1;
        }
        counter[1] += b->steps[1];
        if (counter[1] >= b->stepEventCount) {
            counter[1] -= b->stepEventCount;
            hal::fastWrite(PIN_Y_STEP, true);
            positionSteps[1] += (bits & 0x02) ? -1 : 1;
        }
        counter[2] += b->steps[2];
        if (counter[2] >= b->stepEventCount) {
            counter[2] -= b->stepEventCount;
            hal::fastWrite(PIN_Z_STEP, true);
            positionSteps[2] += (bits & 0x04) ? -1 : 1;
        }
        counter[3] += b->steps[3];
        if (counter[3] >= b->stepEventCount) {
            counter[3] -= b->stepEventCount;
            hal::fastWrite(PIN_E_STEP, true);
            positionSteps[3] += (bits & 0x08) ? -1 : 1;
        }
        ++completed;

        // --- velocita' del prossimo step: sempre limitata in accelerazione ---
        const uint32_t delta = b->accelK * timerTicks; // variazione di velocita' maturata nell'ultimo intervallo
        uint16_t rate = (uint16_t) (rateFixed >> 16);
        bool rateChanged = true;

        if (holding) {
            if (rateFixed > delta + ((uint32_t) MotionConfig::MIN_STEP_RATE << 16)) {
                rateFixed -= delta;
            } else {
                // abbastanza lenti da fermarsi: la posizione contata e' quella reale
                hal::fastWrite(PIN_X_STEP, false);
                hal::fastWrite(PIN_Y_STEP, false);
                hal::fastWrite(PIN_Z_STEP, false);
                hal::fastWrite(PIN_E_STEP, false);
                stopNow();
                hal::stepTimer::setInterval(IDLE_TICKS);
                return;
            }
        } else if (completed > b->decelerateAfter) {
            const uint32_t floorRate = (uint32_t) b->finalRate << 16;
            rateFixed = (rateFixed > delta + floorRate) ? rateFixed - delta : floorRate;
        } else if (rate < b->nominalRate) {
            rateFixed += delta;
            if ((uint16_t) (rateFixed >> 16) >= b->nominalRate) rateFixed = (uint32_t) b->nominalRate << 16;
        } else {
            rateChanged = false; // velocita' costante: nessuna divisione
        }

        if (rateChanged) {
            rate = (uint16_t) (rateFixed >> 16);
            if (rate < MotionConfig::MIN_STEP_RATE) rate = MotionConfig::MIN_STEP_RATE;
            timerTicks = ticksForRate(rate);
        }

        // fine impulso: i calcoli qui sopra garantiscono la durata minima richiesta dai driver
        hal::fastWrite(PIN_X_STEP, false);
        hal::fastWrite(PIN_Y_STEP, false);
        hal::fastWrite(PIN_Z_STEP, false);
        hal::fastWrite(PIN_E_STEP, false);

        if (completed >= b->stepEventCount) {
            lastCompleted = b->commandNumber;
            current = nullptr;
            Planner::releaseBlock();
        }

        hal::stepTimer::setInterval(timerTicks);
    }
}
