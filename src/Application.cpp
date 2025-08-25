#include <Arduino.h>
#include "./include/Application.hpp"

#include "./include/SerialCommandReceiver.hpp"
#include "./include/SafetyManager.hpp"
#include "./include/WatchdogHandler.hpp"
#include "./include/StateMachine.hpp"

// Controller
#include "./include/controllers/MotionController.hpp"
#include "./include/controllers/TempController.hpp"
#include "./include/controllers/FanController.hpp"
#include "./include/controllers/EndstopController.hpp"
#include "./include/controllers/SystemController.hpp"
#include "./include/controllers/ExtruderController.hpp"

// Config
#include <avr/wdt.h>

#include "./include/Config.hpp"

#include "./include/BusyHandler.hpp"
#include "include/TimeUtils.hpp"

namespace {
    bool systemReady = false;
    unsigned long lastReadyMessage = 0;
    const unsigned long READY_MESSAGE_INTERVAL = 5000; // 5 secondi
}

void Application::init() {
    Serial.begin(115200);
    while (!Serial); // attende la seriale solo su USB native

    Serial.println(F("Avvio firmware 3DP..."));

    // Inizializzazione moduli core
    WatchdogHandler::enable();
    SafetyManager::init();
    StateMachine::setState(MachineState::Idle);

    // Inizializzazione hardware/controller
    MotionController::init();
    ExtruderController::init();
    TempController::init();
    FanController::init();
    EndstopController::init();

    TempController::enableMonitoring(TEMP_MONITORING_ENABLED);

    MCUSR &= ~(1 << WDRF);
    wdt_disable();

    Serial.println(F("Sistema pronto."));
    lastReadyMessage = millis();
}

void Application::loop() {
    WatchdogHandler::reset();

    if (!systemReady && TimeUtils::hasElapsed(lastReadyMessage, READY_MESSAGE_INTERVAL)) {
        Serial.println(F("Sistema pronto."));
        lastReadyMessage = millis();
    }

    SerialCommandReceiver::update();
    SafetyManager::update();
    BusyHandler::update();
}

void Application::notifyCommandReceived() {
    systemReady = true;
}
