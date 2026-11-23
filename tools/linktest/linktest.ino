// Sketch di misura del collegamento: nessuna logica di stampa.
// Riceve righe terminate da '\n' e risponde con un ack di lunghezza scelta.
// Il primo carattere della riga dice cosa fare con il contenuto:
//   'a' = ASCII: converte tutti gli interi della riga (come farebbe il parser dei comandi)
//   'f' = ASCII: converte i numeri con strtod (virgola mobile, come oggi)
//   'b' = binario: il secondo byte e' la lunghezza, seguono i byte grezzi (nessuna conversione)
// Il baud si sceglie con BAUD in compilazione.
#ifndef BAUD
#define BAUD 115200
#endif
#ifndef ACK_LEN
#define ACK_LEN 16
#endif

static char line[96];
static uint8_t len = 0;
static volatile long sink;

static void ack() {
  // ack di lunghezza fissa, ultimo carattere '\n'
  static const char text[] = "OK0 N12345 D12345 PADDINGPADDING";
  Serial.write((const uint8_t *) text, ACK_LEN - 1);
  Serial.write('\n');
}

void setup() {
  Serial.begin(BAUD);
  Serial.write("READY\n");
}

void loop() {
  while (Serial.available()) {
    const char c = (char) Serial.read();
    if (len == 0 && c == 'b') {
      // frame binario: 'b' <n> <n byte>
      while (!Serial.available()) {}
      uint8_t n = (uint8_t) Serial.read();
      long sum = 0;
      while (n--) {
        while (!Serial.available()) {}
        sum += (uint8_t) Serial.read();
      }
      sink = sum;
      ack();
      continue;
    }
    if (c != '\n') {
      if (len < sizeof(line) - 1) line[len++] = c;
      continue;
    }
    line[len] = '\0';
    if (line[0] == 'a') {
      long sum = 0;
      char *p = line + 1;
      while (*p) {
        if (*p == '-' || (*p >= '0' && *p <= '9')) sum += strtol(p, &p, 10);
        else ++p;
      }
      sink = sum;
    } else if (line[0] == 'f') {
      double sum = 0;
      char *p = line + 1;
      while (*p) {
        if (*p == '-' || (*p >= '0' && *p <= '9')) sum += strtod(p, &p);
        else ++p;
      }
      sink = (long) sum;
    }
    len = 0;
    ack();
  }
}
