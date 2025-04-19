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
    void handle(int code, const char *params);

    /**
     * @brief Stampa sul log seriale lo stato della macchina.
     */
    void printStatus();

    /**
     * @brief Esegue un reset soft del sistema (logico).
     */
    void reset();
}
