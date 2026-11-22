// TempController.hpp - Hotend: lettura, regolazione e protezioni termiche

#pragma once

#include <stdint.h>

/**
 * @brief Gestisce la lettura della temperatura e il controllo termico con protezioni.
 *
 * La regolazione e' on/off con isteresi ed e' eseguita da update(), chiamato
 * da SafetyManager::update() (quindi anche durante i movimenti bloccanti).
 * Senza sensore (TEMP_MONITORING_ENABLED = false) il riscaldamento e' rifiutato.
 */
namespace TempController {
    /**
     * @brief Inizializza pin, ADC e strutture termiche.
     */
    void init();

    /**
     * @brief Esegue un comando termico.
     * @param code Codice del comando:
     *  - 10: SET temperature (T10 S200)
     *  - 20: GET temperature  -> "CRT TMP <attuale> <target>"
     *  - 25: Thermal protection status
     *  - 30: Enable/disable runaway protection (T30 S1/S0)
     *  - 0:  Heater off
     * @param params Parametri del comando
     */
    void handle(uint8_t code, const char *params);

    /**
     * @brief Ultima temperatura letta (°C). Non accede all'ADC.
     */
    float getTemperature();

    float getTargetTemperature();

    /**
     * @brief Imposta una nuova temperatura target.
     * @return false se il target e' fuori range o il sensore non e' disponibile
     */
    bool setTargetTemperature(float temp);

    /** Spegne subito l'heater e azzera il target (uso: emergenze). */
    void forceOff();

    /**
     * @brief Verifica se la temperatura è stabile attorno al target.
     */
    bool isStable();

    /** true se l'estrusione e' permessa (sensore assente = nessun vincolo). */
    bool canExtrude();

    /**
     * @brief Abilita o disabilita il controllo della temperatura
     */
    void enableMonitoring(bool enabled);

    /**
     * @brief Lettura sensore, regolazione e protezioni. Si auto-limita a 4 Hz.
     */
    void update();
}
