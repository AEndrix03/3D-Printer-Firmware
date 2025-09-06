//
// Created by redeg on 02/09/2025.
//

#include "./include/CompactResponse.hpp"
#include "./include/hal/McuHAL.hpp"

namespace {
    // Calcola checksum senza strlen per evitare doppio attraversamento
    uint8_t computeChecksum(const char *str) {
        uint8_t cs = 0;
        for (const char *p = str; *p; ++p) {
            cs ^= (uint8_t) *p;
        }
        return cs;
    }
}

namespace CompactResponse {
    void send(ResponseCode code, uint32_t cmdNumber) {
        char buffer[24];

        switch (code) {
            case OK:
                snprintf_P(buffer, sizeof(buffer), PSTR("OK0 N%lu"), cmdNumber);
                break;
            case CHECKSUM_ERROR:
                snprintf_P(buffer, sizeof(buffer), PSTR("E01 N%lu"), cmdNumber);
                break;
            case BUFFER_OVERFLOW_ERROR:
                snprintf_P(buffer, sizeof(buffer), PSTR("E02 N%lu"), cmdNumber);
                break;
            case DUPLICATE_COMMAND_ERROR:
                snprintf_P(buffer, sizeof(buffer), PSTR("E03 N%lu"), cmdNumber);
                break;
            case RESEND_COMMAND_ERROR:
                snprintf_P(buffer, sizeof(buffer), PSTR("E04 N%lu"), cmdNumber);
                break;
            case INVALID_CATEGORY_ERROR:
                snprintf_P(buffer, sizeof(buffer), PSTR("E05 N%lu"), cmdNumber);
                break;
            case BLOCKED_MOTION_ERROR:
                snprintf_P(buffer, sizeof(buffer), PSTR("EM0 N%lu"), cmdNumber);
                break;
            case BLOCKED_TEMP_ERROR:
                snprintf_P(buffer, sizeof(buffer), PSTR("ET0 N%lu"), cmdNumber);
                break;
            case CANCELLED_ERROR:
                snprintf_P(buffer, sizeof(buffer), PSTR("ES0 N%lu"), cmdNumber);
                break;
            case NO_ERROR:
                snprintf_P(buffer, sizeof(buffer), PSTR("ES1 N%lu"), cmdNumber);
                break;
        }

        uint8_t checksum = computeChecksum(buffer);
        hal::serial->print(buffer);
        hal::serial->print(F(" *"));
        hal::serial->println(checksum);
    }

    void sendNoCommand(ResponseCode code) {
        send(code, 0);
    }

    void sendData(const char *dataType, const char *payload) {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%s %s", dataType, payload);

        uint8_t checksum = computeChecksum(buffer);
        hal::serial->print(buffer);
        hal::serial->print(F(" *"));
        hal::serial->println(checksum);
    }

    void sendCriticalData(const char *dataType, const char *payload, uint8_t maxRetries) {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "CRT %s %s", dataType, payload);

        uint8_t checksum = computeChecksum(buffer);
        uint8_t attempts = 0;

        while (attempts < maxRetries || maxRetries == 0) {
            hal::serial->print(buffer);
            hal::serial->print(F(" *"));
            hal::serial->println(checksum);

            hal::serial->flush();

            // Pulisci il buffer seriale in entrata
            while (hal::serial->available()) {
                hal::serial->read();
            }

            uint32_t start = hal::halMillis();
            while (hal::halMillis() - start < 2000) { // 2s timeout
                if (hal::serial->available() >= 4) {
                    // Leggi e valida ACK senza array temporaneo
                    if (hal::serial->read() == 'A') {
                        uint8_t ackChecksum = 0;
                        for (uint8_t i = 0; i < 3; i++) {
                            char digit = hal::serial->read();
                            if (digit >= '0' && digit <= '9') {
                                ackChecksum = ackChecksum * 10 + (digit - '0');
                            } else {
                                goto retry; // ACK malformato
                            }
                        }

                        if (ackChecksum == checksum) {
                            return; // Success!
                        }
                    }
                    break; // Wrong ACK, retry
                }

                // Aggiungi un piccolo delay per non saturare la CPU
                hal::halDelay(10);
            }
            retry:
            attempts++;
        }
        // Max retries reached - continue anyway
    }
}