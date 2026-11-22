#pragma once

#include <stdint.h>

/**
 * @brief Gestisce la ricezione di comandi via seriale.
 *
 * Formato riga: "N<numero> <categoria><codice> [parametri] *<checksum>".
 * Regole di sequenza:
 *  - N == ultimo + 1  -> eseguito, una sola risposta terminale con lo stesso N
 *  - N == ultimo      -> E03 (duplicato: gia' eseguito, la risposta era andata persa)
 *  - N == 0           -> comando di sincronizzazione: sempre accettato, azzera la sequenza
 *  - altro            -> E04 N<atteso>
 * La riga "!" (o il solo carattere '!' durante un movimento) interrompe il movimento in corso.
 */
namespace SerialCommandReceiver {
    extern uint32_t lastCommandNumber;
    extern uint8_t checksumErrors;  ///< righe scartate per checksum (saturato a 255)
    extern uint8_t overflowErrors;  ///< righe scartate perche' troppo lunghe (saturato a 255)

    /**
     * @brief Deve essere chiamata regolarmente nel loop.
     * Legge i caratteri dalla seriale e, se il comando è completo, lo smista.
     */
    void update();
}
