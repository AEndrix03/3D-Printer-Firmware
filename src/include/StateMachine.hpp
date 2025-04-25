#pragma once

/**
 * @brief Gestione dello stato corrente della macchina (Idle, Print, Error, ecc).
 */
enum class MachineState {
    Idle,
    Homing,
    Printing,
    Paused,
    Error
};

namespace StateMachine {

    /**
     * @brief Imposta un nuovo stato macchina.
     * @param newState Stato desiderato
     */
    void setState(MachineState newState);

    /**
     * @brief Restituisce lo stato attuale.
     * @return Stato macchina corrente
     */
    MachineState getState();

    /**
     * @brief Restituisce il nome dello stato come stringa.
     * @return Es. "Idle", "Printing", ...
     */
    const char *getStateName();
}
