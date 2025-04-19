#include <Arduino.h>
#include "./include/SerialCommandReceiver.hpp"
#include "./include/Types.hpp"
#include "./include/CommandDispatcher.hpp"
#include "./include/CommandHistory.hpp"
#include "./include/SafetyManager.hpp"

uint16_t SerialCommandReceiver::lastCommandNumber = 0;

namespace {
    char inputBuffer[64];
    uint8_t bufferIndex = 0;


    uint8_t computeChecksum(const char *str) {
        uint8_t cs = 0;

        // Trova posizione dell'asterisco
        const char *p = strchr(str, '*');
        size_t len = p ? (size_t) (p - str) : strlen(str);

        // Rimuovi eventuali spazi prima del *
        while (len > 0 && str[len - 1] == ' ') {
            --len;
        }

        // Calcola XOR
        for (size_t i = 0; i < len; ++i) {
            cs ^= (uint8_t) str[i];
        }

        return cs;
    }

    uint16_t extractNumber(const char *line) {
        if (line[0] != 'N') return 0;
        return (uint16_t) atoi(line + 1);
    }

    char extractCategory(const char *line) {
        const char *p = strchr(line, ' ');
        return (p && *(p + 1)) ? *(p + 1) : '\0';
    }

    uint8_t extractCode(const char *line) {
        const char *p = strchr(line, ' ');
        return (p && *(p + 2)) ? atoi(p + 2) : 0;
    }

    const char *extractParams(const char *line) {
        const char *p = strchr(line, ' ');
        if (!p) return "";
        p = strchr(p + 1, ' ');
        return p ? (p + 1) : "";
    }

    uint8_t extractProvidedChecksum(const char *line) {
        const char *p = strchr(line, '*');
        return p ? (uint8_t) atoi(p + 1) : 0;
    }

    void resend(uint16_t number) {
        const char *cmd = CommandHistory::get(number);
        if (cmd) {
            Serial.print(F("RESEND N"));
            Serial.println(number);
            Serial.println(cmd);
        } else {
            Serial.print(F("RESEND FAILED N"));
            Serial.println(number);
        }
    }
}

void SerialCommandReceiver::update() {
    while (Serial.available()) {
        char c = Serial.read();

        if (c == '\n' || c == '\r') {
            if (bufferIndex == 0) continue;

            inputBuffer[bufferIndex] = '\0';

            ParsedCommand cmd;
            cmd.number = extractNumber(inputBuffer);
            cmd.category = extractCategory(inputBuffer);
            cmd.code = extractCode(inputBuffer);
            cmd.params = extractParams(inputBuffer);
            cmd.checksum = computeChecksum(inputBuffer);
            cmd.valid = (cmd.checksum == extractProvidedChecksum(inputBuffer));

            if (cmd.valid) {
                if (cmd.number == lastCommandNumber) {
                    Serial.print(F("DUPLICATE N"));
                    Serial.println(cmd.number);
                } else if (cmd.number != lastCommandNumber + 1) {
                    resend(lastCommandNumber + 1);
                } else {
                    lastCommandNumber = cmd.number;
                    SafetyManager::notifyCommandReceived();
                    CommandDispatcher::dispatch(cmd);
                    CommandHistory::store(cmd.number, inputBuffer);
                    Serial.print(F("OK N"));
                    Serial.println(cmd.number);
                }
            } else {
                Serial.print(F("ERR N"));
                Serial.print(cmd.number);
                Serial.print(F(" BAD_CHECKSUM. Requested: "));
                Serial.print(cmd.checksum);
                Serial.print(F(" Extracted: "));
                Serial.println(extractProvidedChecksum(inputBuffer));
            }

            bufferIndex = 0;
        } else if (bufferIndex < sizeof(inputBuffer) - 1) {
            inputBuffer[bufferIndex++] = c;
        } else {
            bufferIndex = 0;
        }
    }
}