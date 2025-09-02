#include <Arduino.h>
#include "./include/SerialCommandReceiver.hpp"
#include "./include/Types.hpp"
#include "./include/CommandDispatcher.hpp"
#include "./include/SafetyManager.hpp"
#include "./include/BusyHandler.hpp"
#include "./include/Application.hpp"
#include "./include/CompactResponse.hpp"

uint32_t SerialCommandReceiver::lastCommandNumber = 0;

namespace {
    char inputBuffer[48];
    uint8_t bufferIndex = 0;
    bool firstCommandReceived = false;

    uint8_t computeChecksum(const char *str) {
        uint8_t cs = 0;
        const char *p = strchr(str, '*');
        size_t len = p ? (size_t) (p - str) : strlen(str);

        while (len > 0 && str[len - 1] == ' ') --len;
        for (size_t i = 0; i < len; ++i) {
            cs ^= (uint8_t) str[i];
        }
        return cs;
    }

    uint32_t extractNumber(const char *line) {
        return (line[0] == 'N') ? atol(line + 1) : 0;
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
        return p ? atoi(p + 1) : 0;
    }

    bool isNextExpected(uint32_t received, uint32_t last) {
        return received == (last + 1);
    }

    bool isDuplicate(uint32_t received, uint32_t last) {
        return received == last;
    }

    void resetBuffer() {
        bufferIndex = 0;
        inputBuffer[0] = '\0';
    }

    bool addCharToBuffer(char c) {
        if (bufferIndex >= sizeof(inputBuffer) - 1) {
            CompactResponse::sendNoCommand(CompactResponse::BUFFER_OVERFLOW_ERROR);
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

            // Skip ACK
            if (inputBuffer[0] == 'A' && isdigit(inputBuffer[1])) {
                resetBuffer();
                continue;
            }

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
                    CompactResponse::send(CompactResponse::DUPLICATE_COMMAND_ERROR, cmd.number);
                } else if (!isNextExpected(cmd.number, lastCommandNumber)) {
                    uint32_t expected = lastCommandNumber + 1;
                    CompactResponse::send(CompactResponse::RESEND_COMMAND_ERROR, expected);
                } else {
                    lastCommandNumber = cmd.number;
                    SafetyManager::notifyCommandReceived();
                    BusyHandler::start();
                    CommandDispatcher::dispatch(cmd);
                    BusyHandler::stop();
                    CompactResponse::send(CompactResponse::OK, cmd.number);
                }
            } else {
                CompactResponse::send(CompactResponse::CHECKSUM_ERROR, cmd.number);
            }

            resetBuffer();
        } else if (!addCharToBuffer(c)) {
            // Buffer overflow handled
        }
    }
}