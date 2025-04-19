#pragma once

/**
 * @brief Gestisce la lettura della temperatura e il controllo termico (es. PID).
 */
namespace TempController {

    /**
     * @brief Inizializza pin, ADC e strutture termiche.
     */
    void init();

    /**
     * @brief Esegue un comando termico (lettura, set, PID, ecc).
     * @param code Codice del comando (es. 10 = SET, 20 = READ)
     * @param params Parametri del comando (es. "T200")
     */
    void handle(int code, const char *params);

    /**
     * @brief Restituisce la temperatura attuale (°C).
     */
    float getTemperature();

    /**
     * @brief Imposta una nuova temperatura target.
     * @param temp Valore in gradi Celsius
     */
    void setTargetTemperature(float temp);

    /**
     * @brief Verifica se la temperatura è stabile attorno al target.
     * @return true se stabile entro un margine ±X °C
     */
    bool isStable();
}
