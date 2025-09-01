#include <Arduino.h>
#include "./include/SerialCommandReceiver.hpp"
#include "./include/Types.hpp"
#include "./include/CommandDispatcher.hpp"
#include "./include/SafetyManager.hpp"
#include "./include/BusyHandler.hpp"
#include "./include/Application.hpp"

uint32_t SerialCommandReceiver::lastCommandNumber = 0;

namespace {
    char inputBuffer[48];
    char outputBuffer[64]; // Buffer per output con checksum
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

    // Nuova funzione per checksum in uscita
    uint8_t computeOutputChecksum(const char *str) {
        uint8_t cs = 0;
        size_t len = strlen(str);

        for (size_t i = 0; i < len; ++i) {
            cs ^= (uint8_t) str[i];
        }
        return cs;
    }

    // Funzione per stampare con checksum automatico
    void printWithChecksum(const char *message) {
        uint8_t checksum = computeOutputChecksum(message);
        snprintf(outputBuffer, sizeof(outputBuffer), "%s**%u", message, checksum);
        Serial.println(outputBuffer);
    }

    // Overload per messaggi con numero
    void printWithChecksum(const char *prefix, uint32_t number) {
        snprintf(outputBuffer, sizeof(outputBuffer), "%s%lu", prefix, number);
        uint8_t checksum = computeOutputChecksum(outputBuffer);
        snprintf(outputBuffer, sizeof(outputBuffer), "%s%lu**%u", prefix, number, checksum);
        Serial.println(outputBuffer);
    }

    uint32_t extractNumber(const char *line) {
        if (line[0] != 'N') return 0;
        return (uint32_t) atol(line + 1);
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

    bool isNextExpected(uint32_t received, uint32_t last) {
        return received == (last + 1);
    }

    bool isDuplicate(uint32_t received, uint32_t last) {
        return received == last;
    }

    void resetBuffer() {
        bufferIndex = 0;
        memset(inputBuffer, 0, sizeof(inputBuffer));
    }

    bool addCharToBuffer(char c) {
        if (bufferIndex >= sizeof(inputBuffer) - 1) {
            printWithChecksum("ERR BUFFER_OVERFLOW");
            resetBuffer();
            return false;
        }
        inputBuffer[bufferIndex++] = c;
        return true;
    }
}

void SerialCommandReceiver::update() {
    while (Serial.available()) {
        char c = Serial.read();

        if (c == '\n' || c == '\r') {
            if (bufferIndex == 0) continue;

            inputBuffer[bufferIndex] = '\0';

            ParsedCommand cmd{};
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
                    printWithChecksum("DUPLICATE N", cmd.number);
                } else if (!isNextExpected(cmd.number, lastCommandNumber)) {
                    uint32_t expected = lastCommandNumber + 1;
                    printWithChecksum("RESEND N", expected);
                } else {
                    lastCommandNumber = cmd.number;
                    SafetyManager::notifyCommandReceived();
                    BusyHandler::start();
                    CommandDispatcher::dispatch(cmd);
                    BusyHandler::stop();
                    printWithChecksum("OK N", cmd.number);
                }
            } else {
                // Messaggio di errore più complesso - uso snprintf
                snprintf(outputBuffer, sizeof(outputBuffer),
                         "ERR N%u BAD_CHECKSUM. Requested: %u Extracted: %u",
                         cmd.number, cmd.checksum, extractProvidedChecksum(inputBuffer));
                uint8_t checksum = computeOutputChecksum(outputBuffer);
                Serial.print(outputBuffer);
                Serial.print("**");
                Serial.println(checksum);
            }

            resetBuffer();
        } else if (!addCharToBuffer(c)) {
            // Buffer overflow gestito in addCharToBuffer()
        }
    }
}