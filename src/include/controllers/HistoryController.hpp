#pragma once

/**
 * @brief Gestione comandi Hxx per la cronologia comandi (CommandHistory).
 */
namespace HistoryController {
    /**
     * @brief Gestisce un comando categoria H con codice specifico.
     * @param code Codice del comando H (10, 20, 30, 40)
     * @param params Parametri (es. "N5" per H20)
     */
    void handle(int code, const char *params);
}
