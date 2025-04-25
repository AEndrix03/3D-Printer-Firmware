#pragma once

/**
 * @brief Gestisce il watchdog hardware del microcontrollore.
 * (Usa WDT su AVR per forzare reset se il firmware si blocca)
 */
namespace WatchdogHandler {

    /**
     * @brief Abilita il watchdog con timeout (es. 2 secondi).
     */
    void enable();

    /**
     * @brief Disabilita il watchdog (con cautela!).
     */
    void disable();

    /**
     * @brief Deve essere chiamato regolarmente per evitare reset (se tutto funziona).
     */
    void reset();  // feed
}
