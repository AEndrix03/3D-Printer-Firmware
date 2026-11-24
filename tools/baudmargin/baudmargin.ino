// Sketch di misura del margine di baud tra la scheda e l'adattatore USB.
//
// La seriale e' impostata esattamente come nel firmware (U2X, divisore calcolato da BAUD).
// I comandi arrivano con il PC al baud "giusto" per la scheda; poi il PC cambia il proprio
// baud e si misura quanti byte arrivano rovinati, nei due versi (vedi measure.py):
//   T  dopo 400 ms invia 100 righe note, un byte dietro l'altro (come le risposte del firmware)
//   G  come T, ma con una pausa di circa un byte tra un byte e l'altro
//   L  invia 6000 righe di fila: il PC ne misura la durata, cioe' il baud reale della scheda
//   R  per 1,5 s conta i byte ricevuti e quelli diversi dalla riga nota, con gli errori di
//      trama (FE) e di sovrascrittura (DOR) segnalati dall'UART
//   ?  riporta i contatori di R
// Opzioni di compilazione: -DBAUD=250000, -DSTOP_BITS=2 (due bit di stop in trasmissione),
// -DDOUBLE_SPEED=0 (16 campioni per bit in ricezione invece di 8).
#include <avr/io.h>
#include <util/delay.h>

#ifndef BAUD
#define BAUD 115200UL
#endif
#ifndef STOP_BITS
#define STOP_BITS 1
#endif
#ifndef DOUBLE_SPEED
#define DOUBLE_SPEED 1
#endif

static const char LINE[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabc\n"; // 40 byte
static const uint8_t LINE_LENGTH = 40;

static uint32_t rxBytes, rxBad, rxFrame, rxOverrun;

static void tx(uint8_t byte) {
  while (!(UCSR0A & _BV(UDRE0))) {}
  UDR0 = byte;
}

static void text(const char *s) {
  while (*s) tx((uint8_t) *s++);
}

static void number(uint32_t value) {
  char digits[11];
  uint8_t n = 0;
  do {
    digits[n++] = (char) ('0' + value % 10);
    value /= 10;
  } while (value);
  while (n) tx((uint8_t) digits[--n]);
}

static void burst(uint16_t lines, bool gaps) {
  while (lines--) {
    for (uint8_t i = 0; i < LINE_LENGTH; ++i) {
      tx((uint8_t) LINE[i]);
      if (gaps) _delay_us(20000000UL / BAUD); // il byte in corso piu' circa un byte di linea a riposo
    }
  }
}

/** Timer1 a 15625 Hz: attese senza perdere byte in arrivo. */
static void timerRestart() {
  TCCR1A = 0;
  TCCR1B = _BV(CS12) | _BV(CS10);
  TCNT1 = 0;
}

static void count(uint16_t ticks) {
  rxBytes = rxBad = rxFrame = rxOverrun = 0;
  uint8_t position = 0;
  timerRestart();
  while (TCNT1 < ticks) {
    const uint8_t status = UCSR0A;
    if (!(status & _BV(RXC0))) continue;
    const uint8_t byte = UDR0;
    ++rxBytes;
    if (status & _BV(FE0)) ++rxFrame;
    if (status & _BV(DOR0)) ++rxOverrun;
    if (byte != (uint8_t) LINE[position]) ++rxBad;
    position = byte == '\n' ? 0 : (uint8_t) ((position + 1) % LINE_LENGTH); // riallinea a ogni fine riga
  }
}

int main() {
#if DOUBLE_SPEED
  UCSR0A = _BV(U2X0);
  UBRR0 = (uint16_t) ((F_CPU / 4 / BAUD - 1) / 2);
#else
  UCSR0A = 0;
  UBRR0 = (uint16_t) ((F_CPU / 8 / BAUD - 1) / 2);
#endif
  UCSR0C = _BV(UCSZ01) | _BV(UCSZ00) | (STOP_BITS == 2 ? _BV(USBS0) : 0);
  UCSR0B = _BV(RXEN0) | _BV(TXEN0);
  text("READY\n");

  for (;;) {
    if (!(UCSR0A & _BV(RXC0))) continue;
    const uint8_t command = UDR0;
    if (command == 'T' || command == 'G') {
      timerRestart();
      while (TCNT1 < 6250) {} // 400 ms: il PC cambia baud
      burst(100, command == 'G');
    } else if (command == 'L') {
      timerRestart();
      while (TCNT1 < 6250) {}
      burst(6000, false);
    } else if (command == 'R') {
      count(23437); // 1,5 s
    } else if (command == '?') {
      text("RX ");
      number(rxBytes);
      tx(' ');
      number(rxBad);
      tx(' ');
      number(rxFrame);
      tx(' ');
      number(rxOverrun);
      tx('\n');
    }
  }
}
