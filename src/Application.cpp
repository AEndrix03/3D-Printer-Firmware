// Application.cpp - Updated to use HAL
#include "./include/Application.hpp"
#include "./include/hal/McuHAL.hpp"
#include "./include/SerialCommandReceiver.hpp"
#include "./include/SafetyManager.hpp"
#include "./include/WatchdogHandler.hpp"
#include "./include/StateMachine.hpp"

// Controllers
#include "./include/controllers/MotionController.hpp"
#include "./include/controllers/TempController.hpp"
#include "./include/controllers/FanController.hpp"
#include "./include/controllers/EndstopController.hpp"
#include "./include/controllers/SystemController.hpp"
#include "./include/controllers/ExtruderController.hpp"

#include "./include/Config.hpp"
#include "./include/BusyHandler.hpp"
#include "./include/TimeUtils.hpp"

namespace {
    bool systemReady = false;
    unsigned long lastReadyMessage = 0;
    unsigned long bootTime = 0;
    constexpr unsigned long READY_MESSAGE_INTERVAL = 5000;
    constexpr unsigned long BOOT_GRACE_PERIOD = 10000; // 10 secondi di grazia all'avvio
}

void Application::init() {
    bootTime = hal::halMillis();

    // Initialize HAL first
    hal::init();

    // Initialize serial through HAL
    hal::serial->begin(115200);

    hal::serial->println(F("Avvio firmware 3DP..."));
    hal::serial->print(F("Platform: "));
    hal::serial->println(hal::getPlatformName());
    hal::serial->print(F("Free heap: "));
    hal::serial->print(hal::getFreeHeap());
    hal::serial->print(F(" / "));
    hal::serial->println(hal::getTotalHeap());

    // Initialize core modules
    WatchdogHandler::enable();
    SafetyManager::init();
    StateMachine::setState(MachineState::Idle);

    // Initialize hardware/controllers
    MotionController::init();
    ExtruderController::init();
    TempController::init();
    FanController::init();
    EndstopController::init();

    TempController::enableMonitoring(TEMP_MONITORING_ENABLED);

    // Clear watchdog reset flag (platform specific)
#ifdef __AVR__
    MCUSR &= ~(1 << WDRF);
#endif
    hal::watchdog::disable();
    hal::watchdog::enable(2000); // Re-enable con timeout di 2 secondi

    hal::serial->println(F("Sistema pronto."));
    lastReadyMessage = hal::halMillis();
}

void Application::loop() {
    // Reset watchdog all'inizio di ogni loop
    WatchdogHandler::reset();

    // Calcola tempo dalla boot
    unsigned long uptime = hal::halMillis() - bootTime;

    // Stampa "Sistema pronto" solo se:
    // 1. Non è ancora pronto AND
    // 2. È passato il grace period di boot AND
    // 3. È passato l'intervallo dai messaggi precedenti
    if (!systemReady &&
        uptime > BOOT_GRACE_PERIOD &&
        TimeUtils::hasElapsed(lastReadyMessage, READY_MESSAGE_INTERVAL)) {
        hal::serial->println(F("Sistema pronto."));
        lastReadyMessage = hal::halMillis();
    }

    // Update core systems con watchdog reset intercalati
    SerialCommandReceiver::update();
    WatchdogHandler::reset();

    SafetyManager::update();
    WatchdogHandler::reset();

    BusyHandler::update();
    WatchdogHandler::reset();
}

void Application::notifyCommandReceived() {
    systemReady = true;
}