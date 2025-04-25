#pragma once

/**
 * @brief Costanti di configurazione globale per macchina.
 */

// Termico
constexpr bool TEMP_MONITORING_ENABLED = false;
constexpr float TEMP_MAX = 250.0f;      // Massima temperatura sicura
constexpr float TEMP_HYSTERESIS = 2.0f; // Tolleranza per stabilità
