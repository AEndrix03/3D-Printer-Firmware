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
 * Il movimento arriva gia' pianificato dall'host (vedi Planner::Plan).
 *
 * Sequenza dei sestetti:
 *   intestazione  bit5 = velocita' nominale e accelerazione presenti,
 *                 bit4 = campi larghi, bit3..0 = assi presenti (X, Y, Z, E)
 *   sequenza      numero del comando modulo 64
 *   per ogni asse presente, in ordine X Y Z E:
 *                 step con segno, 2 sestetti (+-2047) oppure 3 se "campi larghi"
 *                 (al massimo +-32767), dal piu' significativo
 *   se bit5:      velocita' nominale, 1/64 mm/s, 3 sestetti
 *                 accelerazione, mm/s^2, 3 sestetti
 *                 (assenti = uguali al movimento precedente)
 *   scala         step dell'asse dominante per mm di percorso, in 1/16, 3 sestetti
 *   ingresso      velocita' massima di ingresso, 1/4 mm/s, 2 sestetti
 *   controllo     12 bit bassi del CRC-16/CCITT di tutti i byte precedenti, 2 sestetti
 *
 * Un segmento corto X Y E a velocita' invariata occupa 15 byte.
 *
 * Risposta a un frame accettato (firmware -> host), 5 byte invece di una riga di testo:
 *   10ssssss  numero del comando modulo 64
 *   10llllll  quanti comandi mancano al completamento (N - D); 63 = 63 o piu'
 *   10cccccc 10cccccc  12 bit bassi del CRC-16/CCITT dei due byte precedenti
 *   '\n'
 * Ogni altro esito (errore, rifiuto, duplicato) resta una riga di testo.
 */
namespace MoveFrame {
    constexpr uint8_t HEADER_MARK = 0xC0;
    constexpr uint8_t HEADER_PLAN = 0x20;
    constexpr uint8_t HEADER_WIDE = 0x10;
    constexpr uint8_t HEADER_AXES = 0x0F;
    constexpr uint8_t SEQUENCE_MASK = 0x3F;
    constexpr uint8_t ACK_LAG_UNKNOWN = 63;
    constexpr uint8_t MAX_LENGTH = 27; // intestazione + sequenza + 4 assi larghi + piano + scala + ingresso + controllo

    inline bool isHeader(uint8_t byte) { return byte >= 0xC0; }

    inline bool isContinuation(uint8_t byte) { return (byte & 0xC0) == 0x80; }

    /** Lunghezza totale del frame, ricavata dalla sola intestazione. */
    inline uint8_t lengthFor(uint8_t header) {
        uint8_t axes = 0;
        for (uint8_t bit = 1; bit < 0x10; bit <<= 1) {
            if (header & bit) ++axes;
        }
        return (uint8_t) (2 + axes * ((header & HEADER_WIDE) ? 3 : 2) + ((header & HEADER_PLAN) ? 6 : 0) + 3 + 2 + 2);
    }

    /** CRC-16/CCITT (stesso algoritmo di _crc_ccitt_update di avr-libc), senza tabelle. */
    inline uint16_t crcUpdate(uint16_t crc, uint8_t data) {
        data ^= (uint8_t) (crc & 0xFF);
        data ^= (uint8_t) (data << 4);
        return (uint16_t) ((((uint16_t) data << 8) | (crc >> 8)) ^ (uint8_t) (data >> 4) ^ ((uint16_t) data << 3));
    }
}
