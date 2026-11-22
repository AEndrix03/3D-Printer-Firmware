#pragma once

#include <Arduino.h>

namespace SafetyManager {
    void init();

    void update();

    void notifyCommandReceived();

    /**
     * @brief Arresto di emergenza: heater off, stato Error. Idempotente:
     * se un errore e' gia' attivo resta valida la prima causa.
     * @param reason stringa in flash (PSTR), max 11 caratteri
     */
    void emergencyStop(PGM_P reason);

    bool clearError();

    const char *getErrorReason();

    /** Millisecondi trascorsi dall'errore attivo (0 se nessun errore). */
    uint32_t getErrorTimestamp();

    bool isInErrorState();

    bool isAnyEndstopTriggered();

    // Power Management Functions
    enum PowerState {
        ACTIVE = 0,
        IDLE = 1,
        SLEEP = 2,
        DEEP_SLEEP = 3
    };

    /**
     * @brief Gestisce transizioni automatiche tra stati di alimentazione
     */
    void updatePowerManagement();

    /**
     * @brief Forza un specifico stato di alimentazione
     */
    void forcePowerState(PowerState state);

    /**
     * @brief Entra in uno stato di risparmio energetico
     */
    void enterPowerState(PowerState newState);

    /**
     * @brief Riattiva tutti i sistemi dallo stato di risparmio
     */
    void wakeUp();

    /**
     * @brief Restituisce lo stato di alimentazione corrente
     */
    PowerState getPowerState();

    /**
     * @brief Tempo di inattività in secondi
     */
    uint32_t getInactiveTime();
}
