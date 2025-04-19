#pragma once

#include <stdint.h>

/**
 * @brief Memorizza i comandi recenti per supporto a RESEND.
 */
namespace CommandHistory {

    constexpr uint8_t BUFFER_SIZE = 4;

    /**
     * @brief Inizializza il buffer circolare.
     */
    void init();

    /**
     * @brief Memorizza un comando.
     * @param number Numero del comando (Nxxx)
     * @param text Contenuto esatto del comando, es: "N5 M10 X0 Y0 F1000"
     */
    void store(uint16_t number, const char *text);

    /**
     * @brief Recupera un comando memorizzato.
     * @param number Numero del comando richiesto
     * @return Puntatore alla stringa se trovata, nullptr altrimenti
     */
    const char *get(uint16_t number);

    /**
     * @brief Svuota completamente la history.
     */
    void clear();

}
