#include "SimBoard.hpp"
#include "Arduino.h"

#include "../../src/include/Application.hpp"
#include "../../src/include/BusyHandler.hpp"
#include "../../src/include/CompactResponse.hpp"
#include "../../src/include/Config.hpp"
#include "../../src/include/Pins.hpp"
#include "../../src/include/SafetyManager.hpp"
#include "../../src/include/SerialCommandReceiver.hpp"
#include "../../src/include/controllers/TempController.hpp"
#include "../../src/include/hal/McuHAL.hpp"
#include "../../src/include/hal/drivers/StepperConfig.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

namespace {
    using Clock = std::chrono::steady_clock;

    struct RebootSignal {
    };

    // ---- tempo: simulato = base + (reale - realBase) * scala ----
    std::mutex timeMutex;
    Clock::time_point realBase = Clock::now();
    double simBaseMicros = 0.0;
    double timeScale = 1.0;
    double jumpedMicros = 0.0; // somma dei salti artificiali: esclusi dal conteggio del watchdog

    uint64_t simMicros() {
        std::lock_guard<std::mutex> lock(timeMutex);
        const double real = std::chrono::duration<double, std::micro>(Clock::now() - realBase).count();
        return (uint64_t) (simBaseMicros + real * timeScale);
    }

    /// Tempo simulato senza i salti artificiali: e' quello "vissuto" dal firmware.
    uint64_t watchdogClock() {
        std::lock_guard<std::mutex> lock(timeMutex);
        const double real = std::chrono::duration<double, std::micro>(Clock::now() - realBase).count();
        return (uint64_t) (simBaseMicros + real * timeScale - jumpedMicros);
    }

    void rebaseTime(double addMicros, double newScale) {
        std::lock_guard<std::mutex> lock(timeMutex);
        const auto now = Clock::now();
        const double real = std::chrono::duration<double, std::micro>(now - realBase).count();
        jumpedMicros += addMicros;
        simBaseMicros += real * timeScale + addMicros;
        realBase = now;
        timeScale = newScale;
    }

    // ---- seriale ----
    std::mutex serialMutex;
    std::condition_variable hostCv;
    std::deque<char> toFirmware;
    std::deque<char> toHost;

    // ---- pin ----
    std::mutex pinMutex;
    bool pins[32] = {false};
    int analogValues[32] = {0};
    long rawSteps[4] = {0, 0, 0, 0};
    bool recording = false;
    std::vector<std::pair<long, long>> recorded;
    bool recordingIntervals = false;
    bool steppedSinceInterval = false;
    std::vector<uint16_t> recordedIntervals;

    const uint8_t STEP_PINS[4] = {PIN_X_STEP, PIN_Y_STEP, PIN_Z_STEP, PIN_E_STEP};
    const uint8_t DIR_PINS[4] = {PIN_X_DIR, PIN_Y_DIR, PIN_Z_DIR, PIN_E_DIR};
    const bool INVERTED[4] = {StepperConfig::STEPPER_INVERT_X, StepperConfig::STEPPER_INVERT_Y,
                              StepperConfig::STEPPER_INVERT_Z, StepperConfig::STEPPER_INVERT_E};

    long logicalSteps(int axis) {
        return INVERTED[axis] ? -rawSteps[axis] : rawSteps[axis];
    }

    const uint8_t ENDSTOP_PINS[3] = {PIN_ENDSTOP_X, PIN_ENDSTOP_Y, PIN_ENDSTOP_Z};
    bool endstopActive[3] = {false, false, false};
    long endstopAt[3] = {0, 0, 0};

    // ---- controllo del firmware ----
    std::thread firmwareThread;
    std::atomic<bool> running{false};
    std::atomic<bool> frozen{false};
    std::atomic<int> pendingReboot{-1};
    std::atomic<int> bites{0};
    std::atomic<bool> stackGuardBroken{false};
    std::atomic<bool> abortFlag{false};
    hal::ResetCause resetCause = hal::RESET_POWERON;

    // ---- interrupt simulati: un mutex ricorsivo fa da "interrupt disabilitati" ----
    std::recursive_mutex irqMutex;
    std::thread timerThread;
    std::atomic<void (*)()> timerHandler{nullptr};
    std::atomic<uint32_t> timerIntervalTicks{2000};
    std::atomic<uint64_t> worstIsrGapMicros{0};

    std::atomic<uint64_t> watchdogTimeoutMicros{0};
    std::atomic<uint64_t> lastFeedMicros{0};

    /// Punto in cui il firmware "puo' essere interrotto": congelamento e riavvio simulati.
    void checkpoint() {
        while (frozen && running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            lastFeedMicros = watchdogClock(); // un firmware congelato non e' un watchdog scattato
        }
        if (pendingReboot >= 0 || !running) throw RebootSignal{};
    }

    /// Emula il Timer1: chiama l'handler degli step agli istanti programmati.
    void timerMain() {
        uint64_t next = simMicros();
        while (running) {
            void (*handler)() = timerHandler.load();
            if (!handler || frozen) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                next = simMicros();
                continue;
            }

            const uint64_t now = simMicros();
            if (now < next) {
                double scale;
                {
                    std::lock_guard<std::mutex> lock(timeMutex);
                    scale = timeScale;
                }
                if ((double) (next - now) / scale > 400.0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                } else {
                    std::this_thread::yield();
                }
                continue;
            }
            if (now - next > 200000) next = now; // salto d'orologio: niente raffica di step

            {
                std::lock_guard<std::recursive_mutex> lock(irqMutex);
                handler();
            }
            next += timerIntervalTicks.load() / 2; // 2 tick = 1 us
        }
    }

    void firmwareMain() {
        Application application;
        while (running) {
            try {
                application.init();
                while (running) {
                    checkpoint();
                    application.loop();
                }
            } catch (const RebootSignal &) {
                if (!running) break;
                // stato che un vero reset azzererebbe
                resetCause = (hal::ResetCause) pendingReboot.exchange(-1);
                SerialCommandReceiver::lastCommandNumber = 0;
                SafetyManager::clearError();
                BusyHandler::stop();
                CompactResponse::takeResult();
                watchdogTimeoutMicros = 0;
                std::lock_guard<std::mutex> lock(serialMutex);
                toFirmware.clear();
            }
        }
    }

}

// ---------------------------------------------------------------------------
// Funzioni Arduino
// ---------------------------------------------------------------------------
unsigned long millis() { return (unsigned long) (uint32_t) (simMicros() / 1000ULL); }

unsigned long micros() { return (unsigned long) (uint32_t) simMicros(); }

void delay(unsigned long ms) {
    const uint64_t end = simMicros() + (uint64_t) ms * 1000ULL;
    while (simMicros() < end) std::this_thread::yield();
}

void delayMicroseconds(unsigned int) {}

void pinMode(uint8_t, uint8_t) {}

void digitalWrite(uint8_t pin, uint8_t value) {
    std::lock_guard<std::mutex> lock(pinMutex);
    const bool level = value != LOW;
    const bool rising = level && !pins[pin];
    pins[pin] = level;
    if (!rising) return;

    for (int axis = 0; axis < 4; ++axis) {
        if (pin != STEP_PINS[axis]) continue;
        rawSteps[axis] += pins[DIR_PINS[axis]] ? 1 : -1;
        steppedSinceInterval = true;
        if (recording && axis < 2) recorded.emplace_back(logicalSteps(0), logicalSteps(1));
    }
}

int digitalRead(uint8_t pin) {
    std::lock_guard<std::mutex> lock(pinMutex);
    for (int axis = 0; axis < 3; ++axis) {
        // finecorsa NC (ENDSTOP_INVERTED): livello alto = premuto
        if (pin == ENDSTOP_PINS[axis]) return endstopActive[axis] && logicalSteps(axis) <= endstopAt[axis];
    }
    return pins[pin] ? HIGH : LOW;
}

int analogRead(uint8_t pin) {
    std::lock_guard<std::mutex> lock(pinMutex);
    return analogValues[pin];
}

void analogWrite(uint8_t pin, int value) {
    std::lock_guard<std::mutex> lock(pinMutex);
    pins[pin] = value >= 128;
}

char *sim_dtostrf(double value, signed char, unsigned char precision, char *out) {
    sprintf(out, "%.*f", (int) precision, value);
    return out;
}

char *sim_ultoa(unsigned long value, char *out, int) {
    sprintf(out, "%lu", value);
    return out;
}

char *sim_ltoa(long value, char *out, int) {
    sprintf(out, "%ld", value);
    return out;
}

// ---------------------------------------------------------------------------
// HAL nativo
// ---------------------------------------------------------------------------
namespace hal {
    namespace serial {
        void begin(uint32_t) {}

        uint8_t available() {
            std::lock_guard<std::mutex> lock(serialMutex);
            return toFirmware.size() > 255 ? 255 : (uint8_t) toFirmware.size();
        }

        int read() {
            std::lock_guard<std::mutex> lock(serialMutex);
            if (toFirmware.empty()) return -1;
            const char c = toFirmware.front();
            toFirmware.pop_front();
            return (unsigned char) c;
        }

        void write(const uint8_t *buffer, uint8_t size) {
            {
                std::lock_guard<std::mutex> lock(serialMutex);
                toHost.insert(toHost.end(), buffer, buffer + size);
            }
            hostCv.notify_all();
        }

        void write(uint8_t byte) { write(&byte, 1); }

        void writeP(const char *flashText) { write((const uint8_t *) flashText, (uint8_t) strlen(flashText)); }

        void flush() {}

        bool takeAbortRequest() { return abortFlag.exchange(false); }
    }

    void halPinMode(uint8_t, HalPinMode) {}

    void writeDigital(uint8_t pin, bool value) { ::digitalWrite(pin, value ? HIGH : LOW); }

    bool readDigital(uint8_t pin) { return ::digitalRead(pin) == HIGH; }

    int readAnalog(uint8_t pin) { return ::analogRead(pin); }

    void writePwm(uint8_t pin, uint8_t value) { ::analogWrite(pin, value); }

    void setupPwm(uint8_t) {}

    uint32_t halMillis() { return (uint32_t) ::millis(); }

    uint32_t halMicros() { return (uint32_t) ::micros(); }

    void halDelay(uint32_t ms) { ::delay(ms); }

    void delayMicroseconds(uint32_t) {}

    namespace watchdog {
        void enable(uint16_t timeout_ms) {
            watchdogTimeoutMicros = (uint64_t) timeout_ms * 1000ULL;
            lastFeedMicros = watchdogClock();
        }

        void disable() { watchdogTimeoutMicros = 0; }

        void reset() {
            checkpoint();
            const uint64_t now = watchdogClock();
            const uint64_t last = lastFeedMicros.load();
            if (watchdogTimeoutMicros && now > last && now - last > watchdogTimeoutMicros) bites++;
            lastFeedMicros = now;
        }
    }

    void init() {}

    uint8_t irqSave() {
        irqMutex.lock();
        return 0;
    }

    void irqRestore(uint8_t) { irqMutex.unlock(); }

    namespace stepTimer {
        void start(void (*handler)()) { timerHandler = handler; }

        uint16_t takeMaxHandlerTicks() { return 0; }

        uint16_t takeOverruns() { return 0; }

        void setInterval(uint16_t ticks) {
            timerIntervalTicks = ticks;
            std::lock_guard<std::mutex> lock(pinMutex);
            if (steppedSinceInterval && recordingIntervals) recordedIntervals.push_back(ticks);
            steppedSinceInterval = false;
        }
    }

    // Sul PC queste misure non hanno senso: valgono solo sull'hardware reale
    uint16_t minFreeRam() { return 0; }

    bool stackGuardIntact() { return !stackGuardBroken.load(); }

    void reset() {
        pendingReboot = RESET_SOFT;
        throw RebootSignal{};
    }

    ResetCause getResetCause() { return resetCause; }

    const char *getPlatformName() { return "Native"; }

    uint32_t getFreeHeap() { return 1500; }

    uint32_t getTotalHeap() { return 2048; }
}

// ---------------------------------------------------------------------------
// API per i test
// ---------------------------------------------------------------------------
namespace sim {
    void start() {
        if (running.exchange(true)) return;
        firmwareThread = std::thread(firmwareMain);
        timerThread = std::thread(timerMain);
    }

    void stop() {
        if (!running.exchange(false)) return;
        frozen = false;
        if (firmwareThread.joinable()) firmwareThread.join();
        if (timerThread.joinable()) timerThread.join();
    }

    void hostWrite(const std::string &bytes) {
        std::lock_guard<std::mutex> lock(serialMutex);
        for (const char c: bytes) {
            // come l'interrupt di ricezione vero: '!' non entra nel buffer
            if (c == '!') abortFlag = true;
            else toFirmware.push_back(c);
        }
    }

    std::string hostReadLine(int timeoutMs) {
        std::unique_lock<std::mutex> lock(serialMutex);
        const auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);

        while (true) {
            for (size_t i = 0; i < toHost.size(); ++i) {
                if (toHost[i] != '\n') continue;
                std::string line(toHost.begin(), toHost.begin() + (long) i);
                toHost.erase(toHost.begin(), toHost.begin() + (long) i + 1);
                while (!line.empty() && line.back() == '\r') line.pop_back();
                if (!line.empty()) return line;
                i = (size_t) -1; // riga vuota: ricomincia dal nuovo inizio
            }
            if (hostCv.wait_until(lock, deadline) == std::cv_status::timeout) return "";
        }
    }

    void setTimeScale(double scale) { rebaseTime(0.0, scale); }

    void advanceTime(uint64_t milliseconds) {
        double scale;
        {
            std::lock_guard<std::mutex> lock(timeMutex);
            scale = timeScale;
        }
        rebaseTime((double) milliseconds * 1000.0, scale);
    }

    void jumpNearMicrosRollover(uint32_t microsBefore) {
        const uint64_t now = simMicros();
        const uint64_t period = 1ULL << 32;
        const uint64_t target = (now / period + 1) * period - microsBefore;
        double scale;
        {
            std::lock_guard<std::mutex> lock(timeMutex);
            scale = timeScale;
        }
        rebaseTime((double) (target - now), scale);
    }

    void jumpNearMillisRollover(uint32_t millisBefore) {
        const uint64_t now = simMicros();
        const uint64_t period = (1ULL << 32) * 1000ULL;
        const uint64_t target = (now / period + 1) * period - (uint64_t) millisBefore * 1000ULL;
        double scale;
        {
            std::lock_guard<std::mutex> lock(timeMutex);
            scale = timeScale;
        }
        rebaseTime((double) (target - now), scale);
    }

    void setFirmwareSequence(uint32_t lastCommandNumber) {
        SerialCommandReceiver::lastCommandNumber = lastCommandNumber;
    }

    void enableThermistor(bool enabled) {
        TempController::enableMonitoring(enabled);
    }

    Steps steps() {
        std::lock_guard<std::mutex> lock(pinMutex);
        return {logicalSteps(0), logicalSteps(1), logicalSteps(2), logicalSteps(3)};
    }

    void resetSteps() {
        std::lock_guard<std::mutex> lock(pinMutex);
        for (long &s: rawSteps) s = 0;
        recorded.clear();
    }

    void recordPath(bool enabled) {
        std::lock_guard<std::mutex> lock(pinMutex);
        recording = enabled;
        if (enabled) recorded.clear();
    }

    std::vector<std::pair<long, long>> path() {
        std::lock_guard<std::mutex> lock(pinMutex);
        return recorded;
    }

    void recordIntervals(bool enabled) {
        std::lock_guard<std::mutex> lock(pinMutex);
        recordingIntervals = enabled;
        if (enabled) recordedIntervals.clear();
    }

    std::vector<uint16_t> intervals() {
        std::lock_guard<std::mutex> lock(pinMutex);
        return recordedIntervals;
    }

    void setEndstop(int axis, long atSteps) {
        std::lock_guard<std::mutex> lock(pinMutex);
        endstopActive[axis] = true;
        endstopAt[axis] = atSteps;
    }

    void clearEndstops() {
        std::lock_guard<std::mutex> lock(pinMutex);
        for (bool &active: endstopActive) active = false;
    }

    bool pinLevel(uint8_t pin) {
        std::lock_guard<std::mutex> lock(pinMutex);
        return pins[pin];
    }

    void setAnalog(uint8_t pin, int value) {
        std::lock_guard<std::mutex> lock(pinMutex);
        analogValues[pin] = value;
    }

    int adcForTemperature(double celsius) {
        const double kelvin = celsius + 273.15;
        const double r = THERMISTOR_R25 * std::exp(THERMISTOR_BETA * (1.0 / kelvin - 1.0 / 298.15));
        return (int) std::lround(1023.0 * r / (r + THERMISTOR_PULLUP));
    }

    void freeze(bool value) {
        frozen = value;
        // il firmware si ferma al prossimo punto di controllo: gli si lascia il tempo di arrivarci
        if (value) std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    void reboot(uint8_t cause) { pendingReboot = cause; }

    int watchdogBites() { return bites.load(); }

    void breakStackGuard(bool broken) { stackGuardBroken = broken; }
}
