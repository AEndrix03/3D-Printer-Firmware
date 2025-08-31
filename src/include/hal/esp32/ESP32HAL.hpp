//
// Created by redeg on 01/09/2025.
//

#pragma once

#include <stdint.h>

/**
 * @brief ESP32-specific HAL implementation
 * Provides ESP32-optimized GPIO, PWM, Serial, and system functions
 */
namespace hal {
    // ===== GPIO =====
    void writeDigital(uint8_t pin, bool value);

    bool readDigital(uint8_t pin);

    int readAnalog(uint8_t pin);

    void setupPwm(uint8_t pin);

    void writePwm(uint8_t pin, uint8_t value);

    // ===== Time =====
    void delayMicroseconds(uint32_t us);

    // ===== ESP32 Specific =====

    /**
     * @brief Configure pin as input with pulldown (ESP32 specific)
     */
    void configureInputPulldown(uint8_t pin);

    /**
     * @brief Set CPU frequency (ESP32 specific)
     */
    void setCpuFrequency(uint16_t mhz);

    /**
     * @brief Get ESP32 chip info
     */
    const char *getChipModel();

    uint8_t getChipRevision();

    uint32_t getChipId();

    /**
     * @brief WiFi and Bluetooth control (ESP32 specific)
     */
    void enableWiFi(bool enable);

    void enableBluetooth(bool enable);

    /**
     * @brief Advanced PWM control with ESP32 LEDC
     */
    void setupAdvancedPwm(uint8_t pin, uint8_t channel, uint32_t frequency, uint8_t resolution);

    void writeAdvancedPwm(uint8_t channel, uint32_t duty);
}