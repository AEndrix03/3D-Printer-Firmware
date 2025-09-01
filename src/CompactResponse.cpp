//
// Created by redeg on 02/09/2025.
//

#include "./include/CompactResponse.hpp"

namespace {
    const char CODE_OK0[] PROGMEM = "OK0";
    const char CODE_E01[] PROGMEM = "E01";
    const char CODE_E02[] PROGMEM = "E02";
    const char CODE_E03[] PROGMEM = "E03";
    const char CODE_E04[] PROGMEM = "E04";
    const char CODE_E05[] PROGMEM = "E05";
    const char CODE_EM0[] PROGMEM = "EM0";
    const char CODE_ET0[] PROGMEM = "ET0";
    const char CODE_ES0[] PROGMEM = "ES0";

    const char* const CODE_TABLE[] PROGMEM = {
            CODE_OK0, CODE_E01, CODE_E02,
            CODE_E03, CODE_E04, CODE_E05,
            CODE_EM0,CODE_ET0, CODE_ES0,
    };

    uint8_t computeChecksum(const char *str) {
        uint8_t cs = 0;
        size_t len = strlen(str);
        for (size_t i = 0; i < len; ++i) {
            cs ^= (uint8_t) str[i];
        }
        return cs;
    }
}

namespace CompactResponse {
    void send(ResponseCode code, uint32_t cmdNumber) {
        char buffer[24];
        char codeStr[4];

        strcpy_P(codeStr, (char*)pgm_read_word(&CODE_TABLE[code]));
        snprintf(buffer, sizeof(buffer), "%s N%lu", codeStr, cmdNumber);

        uint8_t checksum = computeChecksum(buffer);
        Serial.print(buffer);
        Serial.print(F(" *"));
        Serial.println(checksum);
    }

    void sendNoCommand(ResponseCode code) {
        send(code, 0);
    }
}