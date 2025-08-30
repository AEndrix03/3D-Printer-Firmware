//
// Created by redeg on 30/08/2025.
//

// PlatformConfig.hpp - Platform-specific pin mappings and configurations

#pragma once

#ifdef PLATFORM_ARDUINO
// Arduino UNO pin mappings
    namespace PinConfig {
        // Motors
        constexpr uint8_t X_STEP = 2;
        constexpr uint8_t X_DIR = 5;
        constexpr uint8_t Y_STEP = 3;
        constexpr uint8_t Y_DIR = 6;
        constexpr uint8_t Z_STEP = 4;
        constexpr uint8_t Z_DIR = 7;
        constexpr uint8_t E_STEP = 12;
        constexpr uint8_t E_DIR = 13;

        // Endstops
        constexpr uint8_t ENDSTOP_X = 9;
        constexpr uint8_t ENDSTOP_Y = 10;
        constexpr uint8_t ENDSTOP_Z = 11;

        // Temperature
        constexpr uint8_t HEATER = A1;
        constexpr uint8_t THERMISTOR = A0;

        // Fan
        constexpr uint8_t FAN = A2;
    }

    namespace SystemConfig {
        constexpr uint32_t SERIAL_BAUD = 115200;
        constexpr uint16_t WATCHDOG_TIMEOUT_MS = 2000;
        constexpr uint16_t MAX_HEAP_BYTES = 2048;
    }

#elif defined(PLATFORM_ESP32)
// ESP32 pin mappings
    namespace PinConfig {
        // Motors - using ESP32 GPIO
        constexpr uint8_t X_STEP = 26;
        constexpr uint8_t X_DIR = 27;
        constexpr uint8_t Y_STEP = 25;
        constexpr uint8_t Y_DIR = 33;
        constexpr uint8_t Z_STEP = 32;
        constexpr uint8_t Z_DIR = 35;
        constexpr uint8_t E_STEP = 14;
        constexpr uint8_t E_DIR = 12;

        // Endstops
        constexpr uint8_t ENDSTOP_X = 34;
        constexpr uint8_t ENDSTOP_Y = 39;
        constexpr uint8_t ENDSTOP_Z = 36;

        // Temperature
        constexpr uint8_t HEATER = 13;
        constexpr uint8_t THERMISTOR = 35; // ADC1_CH7

        // Fan
        constexpr uint8_t FAN = 15;
    }

    namespace SystemConfig {
        constexpr uint32_t SERIAL_BAUD = 115200;
        constexpr uint16_t WATCHDOG_TIMEOUT_MS = 5000;
        constexpr uint32_t MAX_HEAP_BYTES = 327680; // 320KB
        constexpr uint8_t WIFI_ENABLED = 1;
        constexpr uint8_t BLUETOOTH_ENABLED = 0;
    }

#else
#error "Platform not defined! Use -DPLATFORM_ARDUINO or -DPLATFORM_ESP32"
#endif

// Common configuration for all platforms
namespace CommonConfig {
    constexpr float MAX_TEMP_C = 280.0f;
    constexpr float MIN_TEMP_C = 0.0f;
    constexpr float TEMP_HYSTERESIS = 2.0f;

    constexpr float BUILD_VOLUME_X_MM = 200.0f;
    constexpr float BUILD_VOLUME_Y_MM = 200.0f;
    constexpr float BUILD_VOLUME_Z_MM = 180.0f;

    constexpr uint8_t COMMAND_BUFFER_SIZE = 48;
    constexpr uint8_t HISTORY_BUFFER_SIZE = 3;
}