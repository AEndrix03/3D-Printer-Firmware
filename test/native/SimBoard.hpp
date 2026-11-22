// Scheda simulata: sostituisce HAL e funzioni Arduino quando il firmware e'
// compilato con -DPLATFORM_NATIVE. Il firmware gira in un thread, il test
// parla con lui tramite la "seriale" e ispeziona pin e contatori di step.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sim {
    struct Steps {
        long x = 0, y = 0, z = 0, e = 0; // step netti, segno gia' corretto per l'inversione degli assi
    };

    /// Avvia il thread del firmware (Application::init + loop).
    void start();

    /// Ferma il thread del firmware.
    void stop();

    // ---- seriale (lato host) ----
    void hostWrite(const std::string &bytes);

    /// Riga inviata dal firmware, "" se non arriva entro timeoutMs (tempo reale).
    std::string hostReadLine(int timeoutMs);

    // ---- tempo ----
    /// Fattore di accelerazione del tempo visto dal firmware (1 = tempo reale).
    void setTimeScale(double scale);

    /// Sposta in avanti l'orologio del firmware (millis/micros).
    void advanceTime(uint64_t milliseconds);

    /// Porta micros() a `microsBefore` microsecondi dal rollover a 32 bit.
    void jumpNearMicrosRollover(uint32_t microsBefore);

    /// Porta millis() a `millisBefore` millisecondi dal rollover a 32 bit (~49,7 giorni).
    void jumpNearMillisRollover(uint32_t millisBefore);

    // ---- firmware ----
    /// Forza l'ultimo numero comando accettato dal firmware.
    void setFirmwareSequence(uint32_t lastCommandNumber);

    /// Abilita il termistore a runtime (di default Config.hpp lo tiene disabilitato).
    void enableThermistor(bool enabled);

    // ---- hardware ----
    Steps steps();

    void resetSteps();

    /// Registra ogni step (X,Y) eseguito: serve a verificare che le rette siano rette.
    void recordPath(bool enabled);

    std::vector<std::pair<long, long>> path();

    bool pinLevel(uint8_t pin);

    void setAnalog(uint8_t pin, int value);

    /// Valore ADC corrispondente a una temperatura, con le costanti di Config.hpp.
    int adcForTemperature(double celsius);

    // ---- guasti ----
    /// Blocca il firmware (come se fosse piantato o scollegato) finche' non viene riattivato.
    void freeze(bool frozen);

    /// Riavvia il firmware alla prossima occasione, anche a meta' movimento.
    /// cause: valore di hal::ResetCause.
    void reboot(uint8_t cause);

    /// Quante volte il watchdog (2 s) sarebbe scattato. Deve restare 0.
    int watchdogBites();
}
