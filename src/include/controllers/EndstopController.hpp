#pragma once

/**
 * @brief Gestisce la lettura e logica dei finecorsa sugli assi.
 */
namespace EndstopController {

    /**
     * @brief Inizializza i pin dei finecorsa come input (pullup).
     */
    void init();

    /**
     * @brief Esegue un comando specifico per i finecorsa.
     * @param code Codice (es. 10 = READ Z)
     * @param params Parametri (opzionali, es. "Z")
     */
    void handle(int code, const char *params);

    /**
     * @brief Verifica se il finecorsa X è attivo.
     */
    bool isTriggeredX();

    /**
     * @brief Verifica se il finecorsa Y è attivo.
     */
    bool isTriggeredY();

    /**
     * @brief Verifica se il finecorsa Z è attivo.
     */
    bool isTriggeredZ();
}
