#pragma once

#include <stdint.h>

/**
 * @brief Definizione dei pin fisici della macchina.
 */

// Motori
constexpr uint8_t PIN_X_STEP = 2;
constexpr uint8_t PIN_X_DIR = 5;
constexpr uint8_t PIN_Y_STEP = 3;
constexpr uint8_t PIN_Y_DIR = 6;
constexpr uint8_t PIN_Z_STEP = 4;
constexpr uint8_t PIN_Z_DIR = 7;
constexpr uint8_t PIN_E_STEP = 8;
constexpr uint8_t PIN_E_DIR = 11;

// Finecorsa
constexpr uint8_t PIN_ENDSTOP_X = 9;
constexpr uint8_t PIN_ENDSTOP_Y = 10;
constexpr uint8_t PIN_ENDSTOP_Z = 11;

// Heater e termistore
constexpr uint8_t PIN_HEATER = 8;
constexpr uint8_t PIN_THERMISTOR = A0;

// Ventola
constexpr uint8_t PIN_FAN = 7;
