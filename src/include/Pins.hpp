#pragma once

#include <stdint.h>

#ifdef PLATFORM_ESP32
// ESP32 pin mappings
constexpr uint8_t PIN_X_STEP = 26;
constexpr uint8_t PIN_X_DIR = 27;
constexpr uint8_t PIN_Y_STEP = 25;
constexpr uint8_t PIN_Y_DIR = 33;
constexpr uint8_t PIN_Z_STEP = 32;
constexpr uint8_t PIN_Z_DIR = 35;
constexpr uint8_t PIN_E_STEP = 14;
constexpr uint8_t PIN_E_DIR = 12;

// Finecorsa
constexpr uint8_t PIN_ENDSTOP_X = 34;
constexpr uint8_t PIN_ENDSTOP_Y = 39;
constexpr uint8_t PIN_ENDSTOP_Z = 36;

// Heater e termistore
constexpr uint8_t PIN_HEATER = 13;
constexpr uint8_t PIN_THERMISTOR = 35; // ADC1_CH7

// Ventola
constexpr uint8_t PIN_FAN = 15;

#else

#include <Arduino.h>

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
constexpr uint8_t PIN_ENDSTOP_Z = 11;

// Heater e termistore
constexpr uint8_t PIN_HEATER = A1;
constexpr uint8_t PIN_THERMISTOR = A0;

// Ventola
constexpr uint8_t PIN_FAN = A2;
#endif