#pragma once

#include <stdint.h>

/**
 * @brief Hardware Abstraction Layer (HAL) - Interfaccia comune per accesso all'hardware MCU.
 *
 * Implementare questa interfaccia in modo specifico per ogni piattaforma:
 * - ArduinoHAL
 * - ESP32HAL
 * - STM32HAL
 */
namespace hal {

    /**
     * @brief Imposta lo stato logico di un pin digitale.
     * @param pin Numero del pin
     * @param value true = HIGH, false = LOW
     */
    void writeDigital(uint8_t pin, bool value);

    /**
     * @brief Legge lo stato di un pin digitale.
     * @param pin Numero del pin
     * @return true = HIGH, false = LOW
     */
    bool readDigital(uint8_t pin);

    /**
     * @brief Legge un valore analogico da un pin (es. termistore).
     * @param pin Numero del pin analogico
     * @return Valore da 0 a 1023 (per Arduino UNO)
     */
    int readAnalog(uint8_t pin);

    /**
     * @brief Attende un numero di microsecondi.
     * @param us Numero di microsecondi
     */
    void delayMicroseconds(uint32_t us);

    /**
     * @brief Imposta un pin in modalità output PWM.
     * @param pin Pin da usare per PWM
     */
    void setupPwm(uint8_t pin);

    /**
     * @brief Scrive un valore PWM su un pin.
     * @param pin Pin di uscita
     * @param value Valore da 0 (off) a 255 (max duty)
     */
    void writePwm(uint8_t pin, uint8_t value);

    /**
     * @brief Imposta un pin in modalità INPUT_PULLUP (default per finecorsa).
     * @param pin Pin da configurare
     */
    void configureInputPullup(uint8_t pin);
}
