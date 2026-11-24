//
// Created by redeg on 02/09/2025.
//

#pragma once

#include <Arduino.h>

/**
 * @brief Tutto l'output di protocollo verso l'host passa da qui.
 *
 * Tre tipi di riga, tutte chiuse da " *<checksum XOR>":
 *  - risposta terminale:  "OK0 N12 *cs" / "E06 N12 *cs"   (esattamente una per comando)
 *  - dato informativo:    "POS 1.000 2.000 3.000 *cs"
 *  - dato critico:        "CRT TMP 205.4 200.0 *cs"       (ritrasmesso finche' l'host non risponde "A<cs>")
 *
 * Le stringhe costanti vivono in flash (PSTR) e il messaggio viene composto in
 * un unico buffer statico: niente buffer da 64 byte sullo stack.
 */
namespace CompactResponse {
    enum ResponseCode : uint8_t {
        OK = 0,                  // OK0
        CHECKSUM_ERROR,          // E01
        BUFFER_OVERFLOW_ERROR,   // E02
        DUPLICATE_COMMAND_ERROR, // E03
        RESEND_COMMAND_ERROR,    // E04
        INVALID_CATEGORY_ERROR,  // E05
        REJECTED_ERROR,          // E06 comando valido ma rifiutato (dettaglio nella riga "ERR ...")
        BLOCKED_MOTION_ERROR,    // EM0
        BLOCKED_TEMP_ERROR,      // ET0
        CANCELLED_ERROR,         // ES0
        NO_ERROR                 // ES1
    };

    /** Invia una risposta terminale "<codice> N<numero>". */
    void send(ResponseCode code, uint32_t cmdNumber);

    /**
     * @brief Invia "OK0 N<numero> D<completato>".
     * I movimenti rispondono OK quando sono in coda, non quando sono finiti:
     * `completed` dice all'host fino a quale comando l'esecuzione e' davvero terminata
     * (serve per riprendere una stampa dal punto giusto).
     */
    void sendOk(uint32_t cmdNumber, uint32_t completed);

    /**
     * @brief Conferma corta di un movimento binario accettato: 4 byte con il bit 7 a 1
     * (numero modulo 64, comandi ancora da completare, controllo a 12 bit) e fine riga.
     * Vedi MoveFrame.hpp. Gli esiti diversi da OK restano righe di testo.
     */
    void sendMoveAck(uint32_t cmdNumber, uint32_t completed);

    /** Imposta l'esito terminale del comando in esecuzione (default OK). */
    void setResult(ResponseCode code);

    /** Restituisce l'esito terminale e lo riporta a OK. */
    ResponseCode takeResult();

    // ---- composizione messaggi dati -------------------------------------
    void begin(PGM_P type, bool critical = false);

    void addP(PGM_P text);

    void addStr(const char *text);

    void addInt(int32_t value);

    void addFloat(float value, uint8_t decimals);

    void end();

    // ---- scorciatoie ----------------------------------------------------
    void sendData(PGM_P type, PGM_P payload);

    void sendCritical(PGM_P type, PGM_P payload);

    /** Invia "ERR <reason>" e marca il comando come rifiutato (E06). */
    void reject(PGM_P reason);

    /** Numero di messaggi critici mai confermati dall'host (saturato a 255). */
    uint8_t getAckFailures();
}
