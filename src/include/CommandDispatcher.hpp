#pragma once

#include "Types.hpp"

/**
 * @brief Dispatcher centrale che smista i comandi ricevuti ai rispettivi controller (Motion, Temp, ecc).
 *
 * Funziona in base alla categoria (M, T, S, ecc.) e al codice numerico associato.
 */
namespace CommandDispatcher {

    /**
     * @brief Smista il comando parsato al modulo appropriato.
     * @param cmd Comando decodificato, già validato
     */
    void dispatch(const ParsedCommand &cmd);
}
