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
    const unsigned long READY_MESSAGE_INTERVAL = 5000;
}

void Application::init() {
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

    hal::serial->println(F("Sistema pronto."));
    lastReadyMessage = hal::millis();
}

void Application::loop() {
    WatchdogHandler::reset();

    if (!systemReady && TimeUtils::hasElapsed(lastReadyMessage, READY_MESSAGE_INTERVAL)) {
        hal::serial->println(F("Sistema pronto."));
        lastReadyMessage = hal::millis();
    }

    SerialCommandReceiver::update();
    SafetyManager::update();
    BusyHandler::update();
}

void Application::notifyCommandReceived() {
    systemReady = true;
}