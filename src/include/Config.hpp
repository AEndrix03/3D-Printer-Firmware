#pragma once

#include <stdint.h>

/**
 * @brief Costanti di configurazione globale per macchina.
 */

// ---- Termico ---------------------------------------------------------------
// false = nessun termistore collegato: il riscaldamento viene RIFIUTATO
// (senza sensore non esiste regolazione ne' protezione).
constexpr bool TEMP_MONITORING_ENABLED = false;

constexpr float TEMP_MAX = 260.0f;          // Massimo target accettato (°C)
constexpr float TEMP_ABS_MAX = 275.0f;      // Oltre questa lettura: heater off + errore
constexpr float TEMP_HYSTERESIS = 2.0f;     // Banda della regolazione on/off (°C)
constexpr float TEMP_MIN_EXTRUDE = 170.0f;  // Sotto questa temperatura l'estrusione e' bloccata

// Termistore NTC verso GND con pull-up verso 5V (valori tipici 100k B3950 + 4.7k).
// VERIFICARE con l'hardware reale prima di abilitare TEMP_MONITORING_ENABLED.
constexpr float THERMISTOR_BETA = 3950.0f;
constexpr float THERMISTOR_R25 = 100000.0f;
constexpr float THERMISTOR_PULLUP = 4700.0f;

// Protezione thermal runaway
constexpr uint8_t THERMAL_WATCH_PERIOD_S = 40;   // finestra di osservazione
constexpr float THERMAL_MIN_RISE_C = 4.0f;       // salita minima in riscaldamento per finestra
constexpr float THERMAL_RUNAWAY_DELTA_C = 15.0f; // calo massimo tollerato a regime

// ---- Sicurezza host --------------------------------------------------------
// Heater acceso e nessun comando dall'host per questo tempo: heater spento.
constexpr uint32_t HOST_LOST_TIMEOUT_MS = 300000UL;
