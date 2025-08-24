#pragma once

/**
 * @brief Espone comandi di sistema generici (status, reset, info).
 */
namespace SystemController {
    /**
     * @brief Gestisce un comando di sistema.
     * @param code Codice (es. 10 = STATUS, 0 = RESET)
     * @param params Parametri (eventualmente vuoti)
     */
    void handle(uint8_t code, const char *params);

    /**
     * @brief Stampa sul log seriale lo stato della macchina.
     */
    void printStatus();

    /**
     * @brief Esegue un reset brutale del sistema (logico).
     */
    void reset();

    /**
     * @brief Avvia la stampa, impostando lo stato Printing, solo se in Idle
     */
    void startPrint();

    /**
     * @brief Imposta lo stato Pause alla macchina, se in stato di Printing
     */
    void pause();

    /**
     * @brief Ritorna allo stato Printing, se in stato di Paused
     */
    void resume();

    /**
     * @brief Forza il reset della macchina
     */
    void emergencyReset();

    /**
     * @brief Avvia il processo di Homing della macchina, solo se in Idle
     */
    void homing();
}
