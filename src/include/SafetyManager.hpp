#pragma once

namespace SafetyManager {
    void init();

    void update();

    void notifyCommandReceived();

    void emergencyStop(const char *reason);

    bool clearError();

    const char *getErrorReason();

    uint32_t getErrorTimestamp();

    bool isInErrorState();

    bool hasCriticalCondition();

    bool isAnyEndstopTriggered();

    // Power Management Functions
    enum PowerState : uint8_t { ACTIVE = 0, IDLE = 1, SLEEP = 2, DEEP_SLEEP = 3 };

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
     * @brief Restituisce il nome dello stato di alimentazione
     */
    const char *getPowerStateName();

    /**
     * @brief Tempo di inattività in secondi
     */
    uint16_t getInactiveTime();
}
