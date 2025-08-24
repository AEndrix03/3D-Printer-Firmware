#include <Arduino.h>
#include "./include/SerialCommandReceiver.hpp"
#include "./include/Types.hpp"
#include "./include/CommandDispatcher.hpp"
#include "./include/CommandHistory.hpp"
#include "./include/SafetyManager.hpp"
#include "./include/BusyHandler.hpp"
#include "./include/Application.hpp"

uint32_t SerialCommandReceiver::lastCommandNumber = 0;

namespace {
    char inputBuffer[32];
    uint8_t bufferIndex = 0;
    bool firstCommandReceived = false;

    uint8_t computeChecksum(const char *str) {
        uint8_t cs = 0;
        const char *p = strchr(str, '*');
        size_t len = p ? (size_t) (p - str) : strlen(str);

        while (len > 0 && str[len - 1] == ' ') {
            --len;
        }

        for (size_t i = 0; i < len; ++i) {
            cs ^= (uint8_t) str[i];
        }
        return cs;
    }

    uint32_t extractNumber(const char *line) {
        if (line[0] != 'N') return 0;
        return (uint32_t) atol(line + 1); // atoi -> atol per uint32_t
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

    void resend(uint32_t number) {
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

    // Gestione semplificata - no overflow per 4+ miliardi comandi
    bool isNextExpected(uint32_t received, uint32_t last) {
        return received == (last + 1);
    }

    bool isDuplicate(uint32_t received, uint32_t last) {
        return received == last;
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
                if (!firstCommandReceived) {
                    firstCommandReceived = true;
                    Application::notifyCommandReceived();
                }

                if (isDuplicate(cmd.number, lastCommandNumber)) {
                    Serial.print(F("DUPLICATE N"));
                    Serial.println(cmd.number);
                } else if (!isNextExpected(cmd.number, lastCommandNumber)) {
                    uint32_t expected = lastCommandNumber + 1;
                    resend(expected);
                } else {
                    lastCommandNumber = cmd.number;
                    SafetyManager::notifyCommandReceived();
                    BusyHandler::start();
                    CommandDispatcher::dispatch(cmd);
                    BusyHandler::stop();
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
