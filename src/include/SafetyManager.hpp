#pragma once

#include <stdint.h>

/**
 * @brief Gestisce condizioni di sicurezza a runtime: temperatura, timeout, comandi critici.
 */
namespace SafetyManager {

    /**
     * @brief Inizializza il modulo di sicurezza.
     */
    void init();

    /**
     * @brief Deve essere chiamata regolarmente nel loop per monitorare stato e sicurezza.
     */
    void update();

    /**
     * @brief Notifica un comando ricevuto con timestamp aggiornato.
     * Serve per controllare eventuali timeout.
     */
    void notifyCommandReceived();

    /**
     * @brief Restituisce true se è stato raggiunto un timeout senza comandi.
     */
    bool hasTimedOut();

    /**
     * @brief Verifica se ci sono condizioni critiche attive (es. temperatura alta).
     * @return true se è presente una condizione pericolosa
     */
    bool hasCriticalCondition();

    /**
     * @brief Verifica se alcuni endstop sono attivi durante modalità critiche come la stampa.
     * @return true se è attivo uno o più endstop quando lo stato è Printing
     */
    bool isAnyEndstopTriggered();

    /**
     * @brief Applica logica di emergenza in caso di problemi.
     */
    void emergencyStop(const char *reason);
}
