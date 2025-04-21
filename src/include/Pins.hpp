#pragma once

#include <stdint.h>
#include <Arduino.h>

// Motori
constexpr uint8_t PIN_X_STEP = 2;
constexpr uint8_t PIN_X_DIR = 5;
constexpr uint8_t PIN_Y_STEP = 3;
constexpr uint8_t PIN_Y_DIR = 6;
constexpr uint8_t PIN_Z_STEP = 4;
constexpr uint8_t PIN_Z_DIR = 7;
constexpr uint8_t PIN_E_STEP = 12;
constexpr uint8_t PIN_E_DIR = 13;

// Finecorsa
constexpr uint8_t PIN_ENDSTOP_X = 9;
constexpr uint8_t PIN_ENDSTOP_Y = 10;
constexpr uint8_t PIN_ENDSTOP_Z = A3;

// Heater e termistore
constexpr uint8_t PIN_HEATER = A1;
constexpr uint8_t PIN_THERMISTOR = A0;

// Ventola
constexpr uint8_t PIN_FAN = A2;
