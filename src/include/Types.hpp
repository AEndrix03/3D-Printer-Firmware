#pragma once

#include <stdint.h>

/**
 * @brief Struttura che rappresenta un comando seriale parsato.
 */
struct ParsedCommand {
    uint32_t number; ///< Numero progressivo del comando (Nxxx)
    char category; ///< Categoria (es. M, T, S...)
    uint8_t code; ///< Codice del comando (es. 10, 20...)
    const char *params; ///< Puntatore alla stringa parametri (es. "X10 Y20 F1500")
    uint8_t checksum; ///< Checksum ricevuto
    bool valid; ///< True se parsing e checksum sono validi
};
