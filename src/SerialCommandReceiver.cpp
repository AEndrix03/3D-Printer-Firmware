#include <Arduino.h>
#include "./include/SerialCommandReceiver.hpp"
#include "./include/Types.hpp"
#include "./include/CommandDispatcher.hpp"
#include "./include/SafetyManager.hpp"
#include "./include/BusyHandler.hpp"
#include "./include/controllers/MotionController.hpp"
#include "./include/Application.hpp"
#include "./include/CompactResponse.hpp"
#include "./include/hal/McuHAL.hpp"
#include "./include/MoveFrame.hpp"

uint32_t SerialCommandReceiver::lastCommandNumber = 0;
uint8_t SerialCommandReceiver::checksumErrors = 0;
uint8_t SerialCommandReceiver::overflowErrors = 0;

namespace {
    // 64 = dimensione del buffer RX hardware dell'Uno: una riga piu' lunga non
    // potrebbe comunque arrivare integra mentre il firmware e' occupato.
    char inputBuffer[64];
    uint8_t bufferIndex = 0;
    bool lineOverflow = false;
    // Frame binario in arrivo: usa lo stesso buffer della riga di testo (non si sovrappongono mai)
    uint8_t frameLength = 0; // 0 = nessun frame in corso

    void bump(uint8_t &counter) {
        if (counter < 255) counter++;
    }

    void processLine() {
        using namespace SerialCommandReceiver;

        // ACK fuori tempo ("A123"): non e' un comando
        if (inputBuffer[0] == 'A' && isdigit(inputBuffer[1])) return;

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
        MotionController::releaseStream(); // l'host parla in testo: e' riallineato

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

        const CompactResponse::ResponseCode result = CompactResponse::takeResult();
        if (result == CompactResponse::OK) {
            CompactResponse::sendOk(cmd.number, MotionController::lastCompletedCommand(cmd.number));
        } else {
            CompactResponse::send(result, cmd.number);
        }
    }
}

namespace {
    void frameError() {
        using namespace SerialCommandReceiver;
        frameLength = 0;
        bufferIndex = 0;
        bump(checksumErrors);
        CompactResponse::send(CompactResponse::CHECKSUM_ERROR, lastCommandNumber + 1);
    }

    /** Legge un campo con segno di `sextets` sestetti a partire da `at`. */
    int32_t readSigned(uint8_t &at, uint8_t sextets) {
        uint32_t value = 0;
        for (uint8_t i = 0; i < sextets; ++i) value = (value << 6) | ((uint8_t) inputBuffer[at++] & 0x3F);
        const uint32_t sign = (uint32_t) 1 << (sextets * 6 - 1);
        return (value & sign) ? (int32_t) value - (int32_t) (sign << 1) : (int32_t) value;
    }

    /** Legge un campo senza segno; i valori oltre 16 bit diventano 65535. */
    uint16_t readUnsigned(uint8_t &at, uint8_t sextets) {
        uint32_t value = 0;
        for (uint8_t i = 0; i < sextets; ++i) value = (value << 6) | ((uint8_t) inputBuffer[at++] & 0x3F);
        return value > 0xFFFF ? 0xFFFF : (uint16_t) value;
    }

    void processFrame() {
        using namespace SerialCommandReceiver;
        const uint8_t length = frameLength;
        frameLength = 0;
        bufferIndex = 0;

        // --- controllo ---
        uint16_t crc = 0xFFFF;
        for (uint8_t i = 0; i < length - 2; ++i) crc = MoveFrame::crcUpdate(crc, (uint8_t) inputBuffer[i]);
        const uint16_t received = (uint16_t) (((uint8_t) inputBuffer[length - 2] & 0x3F) << 6) |
                                  ((uint8_t) inputBuffer[length - 1] & 0x3F);
        if ((crc & 0x0FFF) != received) {
            bump(checksumErrors);
            CompactResponse::send(CompactResponse::CHECKSUM_ERROR, lastCommandNumber + 1);
            return;
        }

        Application::notifyCommandReceived();

        // --- sequenza (modulo 64) ---
        const uint8_t sequence = (uint8_t) inputBuffer[1] & MoveFrame::SEQUENCE_MASK;
        const uint8_t behind = (uint8_t) (((uint8_t) (lastCommandNumber + 1) - sequence) & MoveFrame::SEQUENCE_MASK);
        if (behind != 0) {
            if (behind <= 8 && behind <= lastCommandNumber) {
                // gia' ricevuto: l'host lo ha rispedito perche' la risposta era andata persa
                CompactResponse::send(CompactResponse::DUPLICATE_COMMAND_ERROR, lastCommandNumber + 1 - behind);
            } else {
                CompactResponse::send(CompactResponse::RESEND_COMMAND_ERROR, lastCommandNumber + 1);
            }
            return;
        }
        const uint32_t number = ++lastCommandNumber;

        // --- campi ---
        const uint8_t header = (uint8_t) inputBuffer[0];
        const uint8_t sextets = (header & MoveFrame::HEADER_WIDE) ? 3 : 2;
        uint8_t at = 2;
        int32_t delta[4] = {0, 0, 0, 0};
        for (uint8_t axis = 0; axis < 4; ++axis) {
            if (header & (uint8_t) (0x08 >> axis)) delta[axis] = readSigned(at, sextets);
        }
        Planner::Plan plan = {0, 0, 0, 0};
        if (header & MoveFrame::HEADER_PLAN) {
            plan.nominalSpeed = readUnsigned(at, 3);
            plan.acceleration = readUnsigned(at, 3);
        }
        plan.stepScale = readUnsigned(at, 3);
        plan.entrySpeed = (uint16_t) (readUnsigned(at, 2) << 4); // da 1/4 a 1/64 mm/s

        // --- esecuzione: esattamente una risposta terminale ---
        CompactResponse::takeResult();
        SafetyManager::notifyCommandReceived();
        BusyHandler::start();
        MotionController::handleStreamMove(delta, plan, number);
        BusyHandler::stop();
        SafetyManager::notifyCommandReceived();

        const CompactResponse::ResponseCode result = CompactResponse::takeResult();
        if (result == CompactResponse::OK) {
            CompactResponse::sendOk(number, MotionController::lastCompletedCommand(number));
        } else {
            CompactResponse::send(result, number);
        }
    }
}

void SerialCommandReceiver::update() {
    MotionController::pollAbort();

    while (hal::serial::available()) {
        const char c = (char) hal::serial::read();
        const uint8_t byte = (uint8_t) c;

        // ---- movimento binario: tutti i byte hanno il bit 7 a 1 ----
        if (byte & 0x80) {
            if (MoveFrame::isHeader(byte)) {
                if (frameLength != 0) frameError(); // il frame precedente e' rimasto a meta'
                lineOverflow = false;
                bufferIndex = 0;
                frameLength = MoveFrame::lengthFor(byte);
                inputBuffer[bufferIndex++] = c;
            } else if (frameLength != 0) {
                inputBuffer[bufferIndex++] = c;
                if (bufferIndex == frameLength) {
                    processFrame();
                    return; // un comando per giro: vedi la nota sulle righe di testo
                }
            }
            // byte di continuazione senza intestazione: resto di un frame gia' scartato
            continue;
        }
        if (frameLength != 0) frameError(); // testo nel mezzo di un frame: il frame e' perso

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
            // Un solo comando per giro, anche se nel buffer ce ne sono altri: tra un comando
            // e il successivo il ciclo principale deve sempre poter azzerare il watchdog e
            // aggiornare sicurezza e temperatura. Con i movimenti inviati in fila il buffer
            // puo' non svuotarsi mai.
            return;
        } else if (lineOverflow) {
            // continua a scartare fino al fine riga
        } else if (bufferIndex >= sizeof(inputBuffer) - 1) {
            lineOverflow = true;
        } else {
            inputBuffer[bufferIndex++] = c;
        }
    }
}
