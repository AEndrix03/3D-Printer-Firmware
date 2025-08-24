// TempController.hpp - Enhanced with Thermal Protection

#pragma once

/**
 * @brief Gestisce la lettura della temperatura e il controllo termico con protezioni.
 */
namespace TempController {
    /**
     * @brief Inizializza pin, ADC e strutture termiche.
     */
    void init();

    /**
     * @brief Esegue un comando termico con protezioni integrate.
     * @param code Codice del comando:
     *  - 10: SET temperature (T10 T200)
     *  - 20: GET temperature  
     *  - 25: Thermal protection status
     *  - 30: Enable/disable protection (T30 S1/S0)
     *  - 0:  Emergency stop
     * @param params Parametri del comando
     */
    void handle(uint8_t code, const char *params);

    /**
     * @brief Restituisce la temperatura attuale (°C).
     */
    float getTemperature();

    /**
     * @brief Imposta una nuova temperatura target con controlli di sicurezza.
     * @param temp Valore in gradi Celsius
     */
    void setTargetTemperature(float temp);

    /**
     * @brief Verifica se la temperatura è stabile attorno al target.
     * @return true se stabile entro un margine ±X °C
     */
    bool isStable();

    /**
     * @brief Abilita o disabilita il controllo della temperatura
     */
    void enableMonitoring(bool enabled);

    /**
     * @brief Controlla thermal runaway e protezioni. Da chiamare nel SafetyManager::update()
     */
    void updateThermalProtection();

    /**
     * @brief Verifica condizioni di thermal runaway
     * @return true se rilevato thermal runaway
     */
    bool checkThermalRunaway();
}
