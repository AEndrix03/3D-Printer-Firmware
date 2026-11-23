#pragma once

#include <stdint.h>

/**
 * @brief Formato binario dei comandi di movimento (host -> firmware).
 *
 * Ogni byte del frame ha il bit 7 a 1 e porta 6 bit utili ("sestetto"):
 *  - 11xxxxxx  primo byte del frame (intestazione)
 *  - 10xxxxxx  byte successivi
 * Un byte di movimento non puo' quindi essere scambiato per testo, per un fine
 * riga o per il carattere di arresto '!', e l'intestazione e' riconoscibile da
 * sola: dopo un errore la ricezione si riallinea al frame successivo.
 *
 * Sequenza dei sestetti:
 *   intestazione  bit5 = velocita' presente, bit4 = campi larghi,
 *                 bit3..0 = assi presenti (X, Y, Z, E)
 *   sequenza      numero del comando modulo 64
 *   per ogni asse presente, in ordine X Y Z E:
 *                 step con segno, 2 sestetti (+-2047) oppure 3 se "campi larghi" (+-131071),
 *                 dal piu' significativo
 *   velocita'     se presente: mm/min interi, 3 sestetti (fino a 262143)
 *   controllo     12 bit bassi del CRC-16/CCITT di tutti i byte precedenti, 2 sestetti
 *
 * Un segmento corto X Y E senza velocita' occupa 10 byte.
 */
namespace MoveFrame {
    constexpr uint8_t HEADER_MARK = 0xC0;
    constexpr uint8_t HEADER_FEED = 0x20;
    constexpr uint8_t HEADER_WIDE = 0x10;
    constexpr uint8_t HEADER_AXES = 0x0F;
    constexpr uint8_t SEQUENCE_MASK = 0x3F;
    constexpr uint8_t MAX_LENGTH = 19; // intestazione + sequenza + 4 assi larghi + velocita' + controllo

    inline bool isHeader(uint8_t byte) { return byte >= 0xC0; }

    inline bool isContinuation(uint8_t byte) { return (byte & 0xC0) == 0x80; }

    /** Lunghezza totale del frame, ricavata dalla sola intestazione. */
    inline uint8_t lengthFor(uint8_t header) {
        uint8_t axes = 0;
        for (uint8_t bit = 1; bit < 0x10; bit <<= 1) {
            if (header & bit) ++axes;
        }
        return (uint8_t) (2 + axes * ((header & HEADER_WIDE) ? 3 : 2) + ((header & HEADER_FEED) ? 3 : 0) + 2);
    }

    /** CRC-16/CCITT (stesso algoritmo di _crc_ccitt_update di avr-libc), senza tabelle. */
    inline uint16_t crcUpdate(uint16_t crc, uint8_t data) {
        data ^= (uint8_t) (crc & 0xFF);
        data ^= (uint8_t) (data << 4);
        return (uint16_t) ((((uint16_t) data << 8) | (crc >> 8)) ^ (uint8_t) (data >> 4) ^ ((uint16_t) data << 3));
    }
}
