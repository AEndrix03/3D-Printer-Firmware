#pragma once

/**
 * @brief Gestisce la ricezione di comandi via seriale.
 * Esegue lettura carattere per carattere e costruisce ParsedCommand.
 */
namespace SerialCommandReceiver {
    extern uint32_t lastCommandNumber;

    /**
     * @brief Deve essere chiamata regolarmente nel loop.
     * Legge i caratteri dalla seriale e, se il comando è completo, lo smista.
     */
    void update();
}
