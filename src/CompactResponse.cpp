//
// Created by redeg on 02/09/2025.
//

#include "./include/CompactResponse.hpp"
#include "./include/hal/McuHAL.hpp"

namespace {
    constexpr uint8_t ACK_MAX_ATTEMPTS = 3;
    constexpr uint16_t ACK_TIMEOUT_MS = 400; // 3 x 400 ms: sempre sotto il watchdog, che viene comunque alimentato

    char txBuf[48];
    uint8_t txLen = 0;
    bool txCritical = false;
    uint8_t ackFailures = 0;
    CompactResponse::ResponseCode pendingResult = CompactResponse::OK;

    const char RESPONSE_CODES[] PROGMEM = "OK0E01E02E03E04E05E06EM0ET0ES0ES1";

    void put(char c) {
        if (txLen < sizeof(txBuf) - 1) txBuf[txLen++] = c;
    }

    void putStr(const char *s) {
        while (*s) put(*s++);
    }

    void putStr_P(PGM_P s) {
        char c;
        while ((c = (char) pgm_read_byte(s++)) != 0) put(c);
    }

    uint8_t checksum() {
        uint8_t cs = 0;
        for (uint8_t i = 0; i < txLen; ++i) cs ^= (uint8_t) txBuf[i];
        return cs;
    }

    void transmit(uint8_t cs) {
        hal::serial->write((const uint8_t *) txBuf, txLen);
        hal::serial->print(F(" *"));
        hal::serial->println((int) cs);
    }

    /**
     * Attende "A<checksum a 3 cifre>". Ritorna true solo se l'ACK corrisponde.
     * Un ACK con checksum diverso significa che l'host ha ricevuto il messaggio
     * corrotto: si ritrasmette subito.
     */
    bool waitForAck(uint8_t cs) {
        const uint32_t start = hal::halMillis();
        uint8_t digits = 0xFF; // 0xFF = in attesa della 'A'
        uint8_t value = 0;

        while (hal::halMillis() - start < ACK_TIMEOUT_MS) {
            hal::watchdog::reset();
            while (hal::serial->available()) {
                const char c = (char) hal::serial->read();
                if (c == 'A') {
                    digits = 0;
                    value = 0;
                } else if (digits != 0xFF && c >= '0' && c <= '9') {
                    value = (uint8_t) (value * 10 + (c - '0'));
                    if (++digits == 3) return value == cs;
                } else {
                    digits = 0xFF;
                }
            }
        }
        return false;
    }
}

namespace CompactResponse {
    void send(ResponseCode code, uint32_t cmdNumber) {
        txLen = 0;
        for (uint8_t i = 0; i < 3; ++i) {
            put((char) pgm_read_byte(RESPONSE_CODES + (uint8_t) code * 3 + i));
        }
        put(' ');
        put('N');
        char num[11];
        ultoa(cmdNumber, num, 10);
        putStr(num);
        transmit(checksum());
    }

    void setResult(ResponseCode code) {
        pendingResult = code;
    }

    ResponseCode takeResult() {
        const ResponseCode r = pendingResult;
        pendingResult = OK;
        return r;
    }

    void begin(PGM_P type, bool critical) {
        txLen = 0;
        txCritical = critical;
        if (critical) putStr_P(PSTR("CRT "));
        putStr_P(type);
    }

    void addP(PGM_P text) {
        put(' ');
        putStr_P(text);
    }

    void addStr(const char *text) {
        put(' ');
        putStr(text);
    }

    void addInt(int32_t value) {
        char num[12];
        ltoa(value, num, 10);
        addStr(num);
    }

    void addFloat(float value, uint8_t decimals) {
        // avr-libc non supporta %f in printf: si usa dtostrf
        if (isnan(value)) value = 0.0f;
        if (value > 999999.0f) value = 999999.0f;
        if (value < -999999.0f) value = -999999.0f;
        char num[16];
        dtostrf(value, 0, decimals, num);
        addStr(num);
    }

    void end() {
        const uint8_t cs = checksum();

        if (!txCritical) {
            transmit(cs);
            return;
        }

        for (uint8_t attempt = 0; attempt < ACK_MAX_ATTEMPTS; ++attempt) {
            // Scarta eventuali byte vecchi: il protocollo e' stop-and-wait,
            // qui l'host non puo' avere comandi validi in volo.
            while (hal::serial->available()) hal::serial->read();

            transmit(cs);
            hal::serial->flush();

            if (waitForAck(cs)) return;
        }

        // Host non raggiungibile: si prosegue, ma l'evento resta diagnosticabile (S11)
        if (ackFailures < 255) ackFailures++;
    }

    void sendData(PGM_P type, PGM_P payload) {
        begin(type);
        addP(payload);
        end();
    }

    void sendCritical(PGM_P type, PGM_P payload) {
        begin(type, true);
        addP(payload);
        end();
    }

    void reject(PGM_P reason) {
        sendData(PSTR("ERR"), reason);
        setResult(REJECTED_ERROR);
    }

    uint8_t getAckFailures() {
        return ackFailures;
    }
}
