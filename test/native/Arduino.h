// Arduino.h minimale per compilare il firmware su PC (-DPLATFORM_NATIVE).
// Serve ai test di integrazione: la logica e' quella vera, l'hardware e' SimBoard.
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdio.h>

#define PROGMEM
#define PSTR(s) (s)
typedef const char *PGM_P;
#define pgm_read_byte(p) (*(const unsigned char *) (p))
#define pgm_read_dword(p) (*(const uint32_t *) (p))
#define strncpy_P strncpy

class __FlashStringHelper;

#define F(s) (reinterpret_cast<const __FlashStringHelper *>(s))

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

enum : uint8_t { A0 = 14, A1, A2, A3, A4, A5 };

unsigned long millis();

unsigned long micros();

void delay(unsigned long ms);

void delayMicroseconds(unsigned int us);

void pinMode(uint8_t pin, uint8_t mode);

void digitalWrite(uint8_t pin, uint8_t value);

int digitalRead(uint8_t pin);

int analogRead(uint8_t pin);

void analogWrite(uint8_t pin, int value);

// Nomi con prefisso per non collidere con le estensioni della libc del PC
char *sim_dtostrf(double value, signed char width, unsigned char precision, char *out);

char *sim_ultoa(unsigned long value, char *out, int base);

char *sim_ltoa(long value, char *out, int base);

#define dtostrf sim_dtostrf
#define ultoa sim_ultoa
#define ltoa sim_ltoa
