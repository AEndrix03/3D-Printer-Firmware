#pragma once

#include <Arduino.h>

/**
 * @brief Scrittura/lettura veloce di pin noti a compile-time.
 *
 * Su ATmega328P (Uno) con un pin costante il compilatore riduce la chiamata a una
 * singola istruzione sbi/cbi (2 cicli, atomica) invece dei ~70 cicli di digitalWrite:
 * indispensabile dentro l'interrupt degli step.
 */
namespace hal {
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega328__) || defined(__AVR_ATmega168__)

    __attribute__((always_inline)) inline void fastWrite(uint8_t pin, bool value) {
        if (pin < 8) {
            if (value) PORTD |= (uint8_t) _BV(pin); else PORTD &= (uint8_t) ~_BV(pin);
        } else if (pin < 14) {
            if (value) PORTB |= (uint8_t) _BV(pin - 8); else PORTB &= (uint8_t) ~_BV(pin - 8);
        } else {
            if (value) PORTC |= (uint8_t) _BV(pin - 14); else PORTC &= (uint8_t) ~_BV(pin - 14);
        }
    }

    __attribute__((always_inline)) inline bool fastRead(uint8_t pin) {
        if (pin < 8) return (PIND & _BV(pin)) != 0;
        if (pin < 14) return (PINB & _BV(pin - 8)) != 0;
        return (PINC & _BV(pin - 14)) != 0;
    }

#else

    inline void fastWrite(uint8_t pin, bool value) { digitalWrite(pin, value ? HIGH : LOW); }

    inline bool fastRead(uint8_t pin) { return digitalRead(pin) == HIGH; }

#endif
}
