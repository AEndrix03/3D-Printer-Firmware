#include <Arduino.h>
#include "./include/SerialCommandReceiver.hpp"
#include "./include/Types.hpp"
#include "./include/CommandDispatcher.hpp"
#include "./include/SafetyManager.hpp"
#include "./include/BusyHandler.hpp"
#include "./include/Application.hpp"
#include "./include/CompactResponse.hpp"
#include "./include/hal/McuHAL.hpp"

uint32_t SerialCommandReceiver::lastCommandNumber = 0;
uint8_t SerialCommandReceiver::checksumErrors = 0;
uint8_t SerialCommandReceiver::overflowErrors = 0;

namespace {
    // 64 = dimensione del buffer RX hardware dell'Uno: una riga piu' lunga non
    // potrebbe comunque arrivare integra mentre il firmware e' occupato.
    char inputBuffer[64];
    uint8_t bufferIndex = 0;
    bool lineOverflow = false;

    void bump(uint8_t &counter) {
        if (counter < 255) counter++;
    }

    void processLine() {
        using namespace SerialCommandReceiver;

        // ACK fuori tempo ("A123"): non e' un comando
        if (inputBuffer[0] == 'A' && isdigit(inputBuffer[1])) return;

        // Abort ricevuto a macchina ferma: niente da interrompere
        if (inputBuffer[0] == '!') return;

        // --- checksum: obbligatorio, calcolato su tutto cio' che precede " *" ---
        char *star = strrchr(inputBuffer, '*');
        uint8_t len = star ? (uint8_t) (star - inputBuffer) : 0;
        while (len > 0 && inputBuffer[len - 1] == ' ') --len;

        uint8_t cs = 0;
        for (uint8_t i = 0; i < len; ++i) cs ^= (uint8_t) inputBuffer[i];

        if (!star || len == 0 || inputBuffer[0] != 'N' || !isdigit(star[1]) || cs != (uint8_t) atoi(star + 1)) {
            bump(checksumErrors);
            CompactResponse::send(CompactResponse::CHECKSUM_ERROR, lastCommandNumber + 1);
            return;
        }
        inputBuffer[len] = '\0'; // i parametri non includono piu' la coda " *cs"

        // --- parsing "N<num> <cat><code> [params]" ---
        ParsedCommand cmd{};
        cmd.number = strtoul(inputBuffer + 1, nullptr, 10);
        cmd.checksum = cs;
        cmd.valid = true;
        cmd.params = "";

        const char *p = strchr(inputBuffer, ' ');
        if (p && p[1]) {
            cmd.category = p[1];
            cmd.code = (uint8_t) atoi(p + 2);
            const char *q = strchr(p + 1, ' ');
            if (q) cmd.params = q + 1;
        }

        Application::notifyCommandReceived();

        // --- sequenza ---
        if (cmd.number == 0) {
            lastCommandNumber = 0; // resync esplicito richiesto dall'host
        } else if (cmd.number == lastCommandNumber) {
            CompactResponse::send(CompactResponse::DUPLICATE_COMMAND_ERROR, cmd.number);
            return;
        } else if (cmd.number != lastCommandNumber + 1) {
            CompactResponse::send(CompactResponse::RESEND_COMMAND_ERROR, lastCommandNumber + 1);
            return;
        } else {
            lastCommandNumber = cmd.number;
        }

        // --- esecuzione: esattamente una risposta terminale ---
        CompactResponse::takeResult();
        SafetyManager::notifyCommandReceived();
        BusyHandler::start();
        CommandDispatcher::dispatch(cmd);
        BusyHandler::stop();
        SafetyManager::notifyCommandReceived(); // l'inattivita' si misura dalla fine del comando
        CompactResponse::send(CompactResponse::takeResult(), cmd.number);
    }
}

void SerialCommandReceiver::update() {
    while (hal::serial->available()) {
        const char c = (char) hal::serial->read();

        if (c == '\n' || c == '\r') {
            if (lineOverflow) {
                // riga troppo lunga: scartata per intero, una sola segnalazione
                lineOverflow = false;
                bufferIndex = 0;
                bump(overflowErrors);
                CompactResponse::send(CompactResponse::BUFFER_OVERFLOW_ERROR, lastCommandNumber + 1);
                continue;
            }
            if (bufferIndex == 0) continue;

            inputBuffer[bufferIndex] = '\0';
            bufferIndex = 0;
            processLine();
        } else if (lineOverflow) {
            // continua a scartare fino al fine riga
        } else if (bufferIndex >= sizeof(inputBuffer) - 1) {
            lineOverflow = true;
        } else {
            inputBuffer[bufferIndex++] = c;
        }
    }
}
